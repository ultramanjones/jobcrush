#include "BrainJobFinder.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>

#include "../aibrain/AiBrain.h"
#include "../aibrain/AiBrainReply.h"
#include "AtsBoardDetector.h"
#include "EmployerBoardHttp.h"
#include "JobScoutReply.h"

BrainJobFinder::BrainJobFinder(AiBrain &aiBrain, QObject *parent)
    : QObject(parent)
    , connectedBrain(aiBrain)
{
}

bool BrainJobFinder::aBrainIsAvailable() const
{
    return connectedBrain.isConfigured();
}

QString BrainJobFinder::brainDisplayName() const
{
    return connectedBrain.selectedProviderDisplayName();
}

QString BrainJobFinder::instructionsFor(const JobLead &jobLead)
{
    QString whatWeKnow;
    if (!jobLead.companyName.trimmed().isEmpty()) {
        whatWeKnow += QStringLiteral("Company: %1\n").arg(jobLead.companyName.trimmed());
    }
    if (!jobLead.positionTitle.trimmed().isEmpty()) {
        whatWeKnow += QStringLiteral("Job title: %1\n").arg(jobLead.positionTitle.trimmed());
    }
    if (!jobLead.discoveryUrl.trimmed().isEmpty()) {
        whatWeKnow += QStringLiteral("Link the user has: %1\n").arg(jobLead.discoveryUrl.trimmed());
    }

    // Plain instructions, a fixed answer shape. The shape is fixed so the
    // reader below can rely on it. The brain is told not to invent a link
    // because a made-up address saved as a real job is the worst outcome
    // here: worse than "not found".
    return QStringLiteral(
        "Find this job posting on the web.\n\n%1\n"
        "Search the web. Find the page for this exact job on the employer's own "
        "website or on the employer's own job board (for example a Greenhouse, "
        "Lever, Ashby, Workday, iCIMS or Taleo page). Do not use LinkedIn, Indeed, "
        "Glassdoor or ZipRecruiter pages. If the link the user has is itself the "
        "employer's own page, read it and use it.\n\n"
        "Answer with ONLY this JSON and nothing else, no words before or after:\n"
        "{\"found\": true or false, \"title\": \"the job title as the employer wrote it\", "
        "\"company\": \"the employer's name\", \"location\": \"city, state or country\", "
        "\"remote\": true or false, \"url\": \"the address of the posting page\", "
        "\"note\": \"one short sentence on what you found or why not\"}\n\n"
        "Never make up a URL. If you cannot find the posting, set found to false "
        "and leave url empty.")
        .arg(whatWeKnow);
}

bool BrainJobFinder::readAnswer(const QString &replyText, JobLead &foundLead, QString &whyNot)
{
    foundLead = JobLead();
    whyNot.clear();

    // Models wrap JSON in a code fence and a sentence no matter what they are
    // told. Take the outermost braces and ignore the rest.
    const int firstBrace = replyText.indexOf(QLatin1Char('{'));
    const int lastBrace = replyText.lastIndexOf(QLatin1Char('}'));
    if (firstBrace < 0 || lastBrace <= firstBrace) {
        whyNot = QStringLiteral("the brain's answer wasn't in the shape Job Crush asked for");
        return false;
    }
    const QString jsonText = replyText.mid(firstBrace, lastBrace - firstBrace + 1);

    QJsonParseError parseError;
    const QJsonObject answer = QJsonDocument::fromJson(jsonText.toUtf8(), &parseError).object();
    if (parseError.error != QJsonParseError::NoError || answer.isEmpty()) {
        whyNot = QStringLiteral("the brain's answer couldn't be read");
        return false;
    }

    const QString note = answer.value(QStringLiteral("note")).toString().simplified();
    const QString url = answer.value(QStringLiteral("url")).toString().trimmed();
    const bool found = answer.value(QStringLiteral("found")).toBool()
        && url.startsWith(QStringLiteral("http"));

    if (!found) {
        whyNot = note.isEmpty() ? QStringLiteral("the brain couldn't find the posting") : note;
        return false;
    }

    // A walled-garden link is not an answer, whatever the brain says.
    AtsBoardDetector boardDetector;
    if (boardDetector.isWalledGarden(url)) {
        whyNot = QStringLiteral("the brain only found it on a site Job Crush can't read");
        return false;
    }

    foundLead.discoveryUrl = url;
    foundLead.positionTitle = answer.value(QStringLiteral("title")).toString().simplified();
    foundLead.companyName = answer.value(QStringLiteral("company")).toString().simplified();
    foundLead.locationText = answer.value(QStringLiteral("location")).toString().simplified();
    foundLead.isRemoteRole = answer.value(QStringLiteral("remote")).toBool();
    foundLead.rawText = note;
    foundLead.boardIdentity = boardDetector.identify(url);
    if (foundLead.boardIdentity.isKnown()) {
        foundLead.postingSource = foundLead.boardIdentity.boardName;
    }
    return true;
}

JobScoutReply *BrainJobFinder::findTheJob(const JobLead &jobLead, QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    if (!aBrainIsAvailable()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "No AI brain is connected, so Job Crush can't search the web for it. "
            "Connect one in Settings to turn that on."), false);
        return scoutReply;
    }

    AiBrainConversationMessage question;
    question.author = AiBrainConversationMessage::Author::Human;
    question.messageText = instructionsFor(jobLead);

    AiBrainRequestOptions requestOptions;
    requestOptions.letTheBrainSearchTheWeb = true;

    AiBrainReply *brainReply = connectedBrain.streamConversation({ question }, scoutReply,
                                                                 requestOptions);
    if (brainReply == nullptr) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "The AI brain isn't ready. Open Settings and check it is connected."), true);
        return scoutReply;
    }

    QObject::connect(brainReply, &AiBrainReply::finished, scoutReply,
                     [scoutReply, brainReply](const QString &completeReplyText) {
        brainReply->deleteLater();
        JobLead foundLead;
        QString whyNot;
        if (!readAnswer(completeReplyText, foundLead, whyNot)) {
            // Not found is an answer, not trouble.
            scoutReply->markFailed(whyNot, false);
            return;
        }
        // The lead rides in a posting's shape so the reply type stays one
        // type. Everything the caller needs is on it.
        JobPosting leadAsPosting;
        leadAsPosting.positionTitle = foundLead.positionTitle;
        leadAsPosting.companyName = foundLead.companyName;
        leadAsPosting.locationText = foundLead.locationText;
        leadAsPosting.isRemoteRole = foundLead.isRemoteRole;
        leadAsPosting.sourceUrl = foundLead.discoveryUrl;
        leadAsPosting.postingSource = foundLead.postingSource;
        leadAsPosting.fullDescriptionText = foundLead.rawText;
        scoutReply->markFinished({ leadAsPosting });
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
