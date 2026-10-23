#include "MoonlightDocumentCleanup.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "../aibrain/AiBrain.h"
#include "../aibrain/AiBrainReply.h"
#include "../exporting/DocumentConverter.h"
#include "ProDocsIntake.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// The most text sent. A resume is a page or two; anything longer is mostly
// not jobs and schooling, and every extra page costs the user money.
constexpr int mostCharactersSent = 30000;

// Words, counted the plain way: runs of letters or digits. Markdown marks
// like # and - are not words, so they do not count.
int wordCountOf(const QString &text)
{
    static const QRegularExpression wordPattern(QStringLiteral("[\\p{L}\\p{N}]+"));
    int wordCount = 0;
    auto matches = wordPattern.globalMatch(text);
    while (matches.hasNext()) {
        matches.next();
        ++wordCount;
    }
    return wordCount;
}

QString instructionsFor(const QString &documentText)
{
    return QStringLiteral(
        "Below is the text of a resume, transcript or similar document. Pull out "
        "every job and every school in it.\n\n"
        "Answer with ONLY this JSON and nothing else, no words before or after:\n"
        "{\"jobs\": [{\"employer\": \"\", \"title\": \"\", \"start\": \"\", \"end\": \"\", "
        "\"summary\": \"\"}], "
        "\"schools\": [{\"school\": \"\", \"credential\": \"\", \"field\": \"\", "
        "\"start\": \"\", \"end\": \"\"}]}\n\n"
        "Rules:\n"
        "- Only use what the text says. Leave a field as an empty string when the "
        "text does not say it. Never guess and never add anything.\n"
        "- One entry per job and one per school. Credit transferred from another "
        "school is its own school entry.\n"
        "- Dates exactly as the text writes them, for example \"Jan 2019\" or "
        "\"2019\" or \"Present\".\n"
        "- summary is one or two sentences from the text about that job, or empty.\n"
        "- Course lists and grades are not schools.\n\n"
        "THE TEXT:\n%1")
        .arg(documentText.left(mostCharactersSent));
}

QString textOf(const QJsonObject &object, const char *name)
{
    return object.value(QLatin1String(name)).toString().simplified();
}

} // namespace

MoonlightDocumentCleanup::MoonlightDocumentCleanup(AiBrain &aiBrain, ProDocsIntake &intake,
                                                   QObject *parent)
    : QObject(parent)
    , connectedBrain(aiBrain)
    , proDocsIntake(intake)
{
}

bool MoonlightDocumentCleanup::aBrainIsAvailable() const
{
    return connectedBrain.isConfigured();
}

bool MoonlightDocumentCleanup::cleanupIsRunning() const
{
    return isRunning;
}

bool MoonlightDocumentCleanup::readAnswer(const QString &replyText,
                                          QList<WorkExperience> &workExperiences,
                                          QList<EducationRecord> &educationRecords,
                                          QString &whyNot)
{
    workExperiences.clear();
    educationRecords.clear();

    const int firstBrace = replyText.indexOf(QLatin1Char('{'));
    const int lastBrace = replyText.lastIndexOf(QLatin1Char('}'));
    if (firstBrace < 0 || lastBrace <= firstBrace) {
        whyNot = QStringLiteral("Moonlight's answer wasn't in the shape Job Crush asked for.");
        return false;
    }
    QJsonParseError parseError;
    const QJsonObject answer = QJsonDocument::fromJson(
        replyText.mid(firstBrace, lastBrace - firstBrace + 1).toUtf8(), &parseError).object();
    if (parseError.error != QJsonParseError::NoError || answer.isEmpty()) {
        whyNot = QStringLiteral("Moonlight's answer couldn't be read.");
        return false;
    }

    for (const QJsonValue &jobValue : answer.value(QStringLiteral("jobs")).toArray()) {
        const QJsonObject jobObject = jobValue.toObject();
        WorkExperience workExperience;
        workExperience.employerName = textOf(jobObject, "employer");
        workExperience.roleTitle = textOf(jobObject, "title");
        workExperience.startDateText = textOf(jobObject, "start");
        workExperience.endDateText = textOf(jobObject, "end");
        workExperience.summaryText = textOf(jobObject, "summary");
        if (!workExperience.employerName.isEmpty() || !workExperience.roleTitle.isEmpty()) {
            workExperiences.append(workExperience);
        }
    }
    for (const QJsonValue &schoolValue : answer.value(QStringLiteral("schools")).toArray()) {
        const QJsonObject schoolObject = schoolValue.toObject();
        EducationRecord educationRecord;
        educationRecord.schoolName = textOf(schoolObject, "school");
        educationRecord.credentialText = textOf(schoolObject, "credential");
        educationRecord.fieldOfStudyText = textOf(schoolObject, "field");
        educationRecord.startDateText = textOf(schoolObject, "start");
        educationRecord.endDateText = textOf(schoolObject, "end");
        if (!educationRecord.schoolName.isEmpty()) {
            educationRecords.append(educationRecord);
        }
    }

    if (workExperiences.isEmpty() && educationRecords.isEmpty()) {
        whyNot = QStringLiteral("Moonlight found no jobs or schools in that document, so "
                                "nothing was changed.");
        return false;
    }
    return true;
}

void MoonlightDocumentCleanup::cleanUpDocument(qint64 professionalDocumentId,
                                               const QString &documentName,
                                               const QString &documentText)
{
    if (isRunning) {
        return;
    }
    if (!aBrainIsAvailable()) {
        emit cleanupStatusChanged(QStringLiteral(
            "No AI brain is connected, so Moonlight can't read this. Connect one in "
            "Settings. Or fix the entries by hand on the Experience and Education tabs."));
        return;
    }
    if (documentText.trimmed().isEmpty()) {
        emit cleanupStatusChanged(QStringLiteral(
            "There are no words in %1 for Moonlight to read.").arg(documentName));
        return;
    }

    AiBrainConversationMessage question;
    question.author = AiBrainConversationMessage::Author::Human;
    question.messageText = instructionsFor(documentText);

    AiBrainReply *brainReply = connectedBrain.streamConversation({ question }, this);
    if (brainReply == nullptr) {
        emit cleanupStatusChanged(QStringLiteral(
            "The AI brain isn't ready. Open Settings and check it is connected."));
        return;
    }

    isRunning = true;
    emit cleanupStatusChanged(QStringLiteral("Moonlight is reading %1…").arg(documentName));

    connect(brainReply, &AiBrainReply::finished, this,
            [this, brainReply, professionalDocumentId, documentName](
                const QString &completeReplyText) {
        brainReply->deleteLater();
        isRunning = false;
        QList<WorkExperience> workExperiences;
        QList<EducationRecord> educationRecords;
        QString whyNot;
        if (!readAnswer(completeReplyText, workExperiences, educationRecords, whyNot)) {
            emit cleanupStatusChanged(whyNot);
            return;
        }
        proDocsIntake.replaceReadingOfDocument(professionalDocumentId, workExperiences,
                                               educationRecords);
        const QString jobsText = workExperiences.count() == 1
            ? QStringLiteral("1 job") : QStringLiteral("%1 jobs").arg(workExperiences.count());
        const QString schoolsText = educationRecords.count() == 1
            ? QStringLiteral("1 school")
            : QStringLiteral("%1 schools").arg(educationRecords.count());
        emit cleanupStatusChanged(QStringLiteral(
            "Moonlight read %1: %2 and %3. They replace Job Crush's own reading of it. "
            "Anything you typed in yourself was kept. Check them on the Experience and "
            "Education tab.")
                .arg(documentName, jobsText, schoolsText));
    });
    connect(brainReply, &AiBrainReply::failed, this,
            [this, brainReply](const QString &whyItFailed) {
        const QString nextStep = brainReply->suggestedNextStep();
        brainReply->deleteLater();
        isRunning = false;
        emit cleanupStatusChanged(nextStep.isEmpty()
                                      ? whyItFailed
                                      : whyItFailed + QLatin1Char(' ') + nextStep);
    });
}

void MoonlightDocumentCleanup::layOutDocument(const QString &documentName,
                                              const QString &documentText,
                                              const QString &destinationFolderPath)
{
    if (isRunning) {
        return;
    }
    if (!aBrainIsAvailable()) {
        emit cleanupStatusChanged(QStringLiteral(
            "No AI brain is connected, so Moonlight can't lay this out. Connect one in "
            "Settings. The plain Word copy still works without one."));
        return;
    }
    if (documentText.trimmed().isEmpty()) {
        emit cleanupStatusChanged(QStringLiteral(
            "There are no words in %1 for Moonlight to lay out.").arg(documentName));
        return;
    }

    AiBrainConversationMessage question;
    question.author = AiBrainConversationMessage::Author::Human;
    question.messageText = QStringLiteral(
        "Below is the text of a document, usually a resume, read out of a PDF or Word "
        "file. Its layout was lost on the way: columns and spacing are gone.\n\n"
        "Lay it out again in Markdown:\n"
        "- The person's name as a # heading at the top.\n"
        "- Each section (Experience, Education, Skills and so on) as a ## heading.\n"
        "- Each job or school as a ### heading with its dates.\n"
        "- Lists as - bullets.\n\n"
        "Rules: keep every word as written. Do not add, remove, reword or correct "
        "anything. Answer with ONLY the Markdown, no words before or after it.\n\n"
        "THE TEXT:\n%1").arg(documentText.left(mostCharactersSent));

    // The whole document comes back, so the answer needs far more room than a
    // chat reply. Some brains (Gemini) also spend part of it thinking first.
    AiBrainRequestOptions layoutOptions;
    layoutOptions.longestAnswerTokens = 12000;
    AiBrainReply *brainReply =
        connectedBrain.streamConversation({ question }, this, layoutOptions);
    if (brainReply == nullptr) {
        emit cleanupStatusChanged(QStringLiteral(
            "The AI brain isn't ready. Open Settings and check it is connected."));
        return;
    }

    isRunning = true;
    emit cleanupStatusChanged(QStringLiteral("Moonlight is laying out %1…").arg(documentName));
    const int wordsSent = wordCountOf(documentText.left(mostCharactersSent));

    connect(brainReply, &AiBrainReply::finished, this,
            [this, brainReply, documentName, destinationFolderPath, wordsSent](
                const QString &completeReplyText) {
        brainReply->deleteLater();
        isRunning = false;

        // Models wrap the answer in a code fence whatever they are told.
        QString markdownText = completeReplyText.trimmed();
        markdownText.remove(QRegularExpression(QStringLiteral("^```[a-zA-Z]*\\s*")));
        markdownText.remove(QRegularExpression(QStringLiteral("\\s*```$")));
        if (markdownText.trimmed().isEmpty()) {
            emit cleanupStatusChanged(QStringLiteral(
                "Moonlight sent nothing back. Try again, or use the plain Word copy."));
            return;
        }

        // An answer cut off in the middle is still an answer, and it would make
        // a Word file with half a resume in it. The words should all come back,
        // so far fewer words than were sent means it stopped early. Headings
        // like "Experience" can be added, and a few stray words can drop, so
        // only a big gap counts.
        const int wordsBack = wordCountOf(markdownText);
        if (wordsBack < wordsSent * 8 / 10) {
            emit cleanupStatusChanged(QStringLiteral(
                "Moonlight's answer was cut off: %1 words came back out of %2. No file was "
                "made. Press the button to try again, or use the plain Word copy.")
                    .arg(wordsBack).arg(wordsSent));
            return;
        }

        QString baseName = QFileInfo(documentName).completeBaseName();
        baseName.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
        baseName = baseName.trimmed().isEmpty() ? QStringLiteral("Document")
                                                : baseName.trimmed();
        const DocumentConverter::ConversionOutcome outcome = DocumentConverter().convertToFile(
            // Year, month, day, hour, minute, so the copies sort by name in
            // the order they were made: Resume_Moonlight_202610162100.docx
            markdownText,
            baseName + QStringLiteral("_Moonlight_")
                + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmm")),
            QStringLiteral("docx"), destinationFolderPath, true);
        if (!outcome.succeeded) {
            emit cleanupStatusChanged(outcome.reasonText + QLatin1Char(' ')
                                      + outcome.whatToDoNextText);
            return;
        }
        emit cleanupStatusChanged(QStringLiteral(
            "Moonlight laid out %1 and saved it as %2. Open it and check it before you "
            "send it: it is a new layout of your words, not a copy of the original page.")
                .arg(documentName, QDir::toNativeSeparators(outcome.writtenFilePath)));
    });
    connect(brainReply, &AiBrainReply::failed, this,
            [this, brainReply](const QString &whyItFailed) {
        const QString nextStep = brainReply->suggestedNextStep();
        brainReply->deleteLater();
        isRunning = false;
        emit cleanupStatusChanged(nextStep.isEmpty()
                                      ? whyItFailed
                                      : whyItFailed + QLatin1Char(' ') + nextStep);
    });
}
