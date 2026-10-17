#include "PastedTextJobReader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "../aibrain/AiBrain.h"
#include "../aibrain/AiBrainReply.h"
#include "EmployerBoardHttp.h"
#include "JobScoutReply.h"

namespace {

// The most text sent to the brain. A long email thread is mostly quoted
// replies, and every extra page costs the user money.
constexpr int mostCharactersSent = 20000;

} // namespace

PastedTextJobReader::PastedTextJobReader(AiBrain &aiBrain)
    : connectedBrain(aiBrain)
{
}

QString PastedTextJobReader::instructionsFor(const QString &pastedText)
{
    return QStringLiteral(
        "Below is text the user pasted. It holds one job: an email, a job alert, a "
        "recruiter message, or a job posting.\n\n"
        "Pull the job out of it. Answer with ONLY this JSON and nothing else, no "
        "words before or after:\n"
        "{\"company\": \"\", \"title\": \"\", \"location\": \"\", \"pay\": \"\", "
        "\"remote\": false, \"url\": \"\", \"posted\": \"YYYY-MM-DD\", "
        "\"description\": \"\"}\n\n"
        "Rules:\n"
        "- Only use what the text says. If the text does not say something, leave "
        "it as an empty string (or false for remote). Never guess and never make "
        "anything up, above all a link.\n"
        "- url is the link to the job posting itself, if the text has one.\n"
        "- posted is the day the job was posted, if the text says it.\n"
        "- description is the job's description as the text writes it, copied, "
        "not rewritten or summed up. Leave out email signatures and replies.\n\n"
        "THE TEXT:\n%1")
        .arg(pastedText.left(mostCharactersSent));
}

bool PastedTextJobReader::readAnswer(const QString &replyText, JobPosting &postingRead,
                                     QString &whyNot)
{
    postingRead = JobPosting();
    whyNot.clear();

    // Models wrap JSON in a code fence and a sentence whatever they are told.
    // Take the outermost braces and ignore the rest.
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

    postingRead.companyName = answer.value(QStringLiteral("company")).toString().simplified();
    postingRead.positionTitle = answer.value(QStringLiteral("title")).toString().simplified();
    postingRead.locationText = answer.value(QStringLiteral("location")).toString().simplified();
    postingRead.salaryText = answer.value(QStringLiteral("pay")).toString().simplified();
    postingRead.isRemoteRole = answer.value(QStringLiteral("remote")).toBool();
    postingRead.fullDescriptionText =
        answer.value(QStringLiteral("description")).toString().trimmed();

    // Only a real web address is kept. Anything else in the url field is the
    // brain filling a box it was told to leave empty.
    const QString url = answer.value(QStringLiteral("url")).toString().trimmed();
    if (url.startsWith(QStringLiteral("http://")) || url.startsWith(QStringLiteral("https://"))) {
        postingRead.sourceUrl = url;
    }

    const QDate postedDate = QDate::fromString(
        answer.value(QStringLiteral("posted")).toString().trimmed(), Qt::ISODate);
    if (postedDate.isValid() && postedDate <= QDate::currentDate()) {
        postingRead.postedTimestamp = QDateTime(postedDate, QTime(12, 0));
    }

    if (postingRead.companyName.isEmpty() && postingRead.positionTitle.isEmpty()) {
        whyNot = QStringLiteral("Moonlight couldn't find a company or a job title in that "
                                "text. Check it is the job's text, or type the two in "
                                "yourself.");
        return false;
    }
    return true;
}

JobScoutReply *PastedTextJobReader::readJob(const QString &pastedText, QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    if (pastedText.trimmed().isEmpty()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "Paste the job's text into the box first."), false);
        return scoutReply;
    }
    if (!connectedBrain.isConfigured()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "No AI brain is connected, so Moonlight can't read the text. Connect one in "
            "Settings, or type the company and the title in yourself."), false);
        return scoutReply;
    }

    AiBrainConversationMessage question;
    question.author = AiBrainConversationMessage::Author::Human;
    question.messageText = instructionsFor(pastedText);

    AiBrainReply *brainReply = connectedBrain.streamConversation({ question }, scoutReply);
    if (brainReply == nullptr) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "The AI brain isn't ready. Open Settings and check it is connected."), true);
        return scoutReply;
    }

    QObject::connect(brainReply, &AiBrainReply::finished, scoutReply,
                     [scoutReply, brainReply](const QString &completeReplyText) {
        brainReply->deleteLater();
        JobPosting postingRead;
        QString whyNot;
        if (!readAnswer(completeReplyText, postingRead, whyNot)) {
            scoutReply->markFailed(whyNot, false);
            return;
        }
        scoutReply->markFinished({ postingRead });
    });
    QObject::connect(brainReply, &AiBrainReply::failed, scoutReply,
                     [scoutReply, brainReply](const QString &whyItFailed) {
        const QString nextStep = brainReply->suggestedNextStep();
        brainReply->deleteLater();
        scoutReply->markFailed(nextStep.isEmpty()
                                   ? whyItFailed
                                   : whyItFailed + QLatin1Char(' ') + nextStep,
                               true);
    });
    return scoutReply;
}
