#include "GreenhouseBoardSource.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"

namespace {

const QString greenhouseApiBase = QStringLiteral("https://boards-api.greenhouse.io/v1/boards/");
const QString greenhouseDisplayName = QStringLiteral("Greenhouse");

// One job out of Greenhouse's JSON.
//
// Greenhouse gives the location as free text and does not say whether a job is
// remote, so that is read from the words. Getting it wrong here only affects
// filtering, and reading "Remote (US)" as remote is right far more often than
// leaving every job unmarked.
JobPosting postingFromGreenhouseJob(const QJsonObject &jobObject, const QString &tenant)
{
    JobPosting posting;

    // toVariant().toLongLong() rather than toDouble(): toDouble() answers 0
    // for anything that is not a JSON number — a missing key, a null, or an id
    // sent as a string — and an id of "0" is worse than no id at all, because
    // every job without one then looks like the same job.
    posting.externalSourceId = QString::number(
        jobObject.value(QStringLiteral("id")).toVariant().toLongLong());
    posting.positionTitle = jobObject.value(QStringLiteral("title")).toString().trimmed();
    posting.sourceUrl = jobObject.value(QStringLiteral("absolute_url")).toString().trimmed();
    posting.locationText =
        jobObject.value(QStringLiteral("location")).toObject()
            .value(QStringLiteral("name")).toString().trimmed();

    // Greenhouse does carry the employer's real name, and it is not the board
    // token. The token for Jane Street's events board is "jshiddenevents";
    // company_name is "Hidden Events". Showing the token would put a
    // lowercase slug on the card where the company should be.
    posting.companyName = jobObject.value(QStringLiteral("company_name")).toString().trimmed();
    if (posting.companyName.isEmpty()) {
        posting.companyName = tenant;
    }

    const QString descriptionHtml = jobObject.value(QStringLiteral("content")).toString();
    posting.fullDescriptionText =
        plainTextFromHtmlFragment(descriptionHtml);

    const QString updatedText = jobObject.value(QStringLiteral("updated_at")).toString();
    if (!updatedText.isEmpty()) {
        posting.postedTimestamp = QDateTime::fromString(updatedText, Qt::ISODate);
    }
    const QString firstPublishedText = jobObject.value(QStringLiteral("first_published")).toString();
    if (!firstPublishedText.isEmpty()) {
        const QDateTime firstPublished = QDateTime::fromString(firstPublishedText, Qt::ISODate);
        if (firstPublished.isValid()) {
            posting.postedTimestamp = firstPublished;
        }
    }

    const QString locationLowered = posting.locationText.toLower();
    posting.isRemoteRole = locationLowered.contains(QStringLiteral("remote"))
                        || locationLowered.contains(QStringLiteral("anywhere"))
                        || locationLowered.contains(QStringLiteral("distributed"));

    posting.discoverySource = AtsBoardName::Greenhouse;
    posting.discoveredTimestamp = QDateTime::currentDateTime();
    return posting;
}

} // namespace

GreenhouseBoardSource::GreenhouseBoardSource() = default;

QString GreenhouseBoardSource::boardName() const
{
    return AtsBoardName::Greenhouse;
}

JobScoutReply *GreenhouseBoardSource::fetchEveryJobForEmployer(const QString &tenant,
                                                               QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    const QString trimmedTenant = tenant.trimmed();
    if (trimmedTenant.isEmpty()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "Job Crush needs the company's Greenhouse name before it can look. Paste a "
            "link to one of their jobs and it will work the name out."));
        return scoutReply;
    }

    const QUrl requestUrl(greenhouseApiBase + QUrl::toPercentEncoding(trimmedTenant)
                          + QStringLiteral("/jobs?content=true"));

    QNetworkReply *networkReply = networkAccessManager.get(employerBoardRequest(requestUrl));
    networkReply->setParent(scoutReply);

    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply, trimmedTenant]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (httpStatusCode == 404) {
            scoutReply->markFailed(noSuchBoardMessage(greenhouseDisplayName, trimmedTenant), false);
            return;
        }
        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                greenhouseDisplayName, networkReply->errorString(),
                httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                greenhouseDisplayName, httpStatusCode, responseBody));
            return;
        }

        const QJsonArray jobArray =
            responseDocument.object().value(QStringLiteral("jobs")).toArray();
        QList<JobPosting> foundPostings;
        for (const QJsonValue &jobValue : jobArray) {
            const JobPosting posting = postingFromGreenhouseJob(jobValue.toObject(), trimmedTenant);

            // No id means Job Crush cannot tell this job from the next one
            // without an id, so on the following sweep they all look like the
            // same job and every one after the first is quietly dropped.
            if (posting.positionTitle.isEmpty()
                    || posting.externalSourceId.isEmpty()
                    || posting.externalSourceId == QStringLiteral("0")) {
                continue;
            }
            foundPostings.append(posting);
        }
        scoutReply->markFinished(foundPostings);
    });

    return scoutReply;
}

JobScoutReply *GreenhouseBoardSource::fetchOneJob(const AtsBoardIdentity &boardIdentity,
                                                  QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    if (!boardIdentity.namesOneJob()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "That link doesn't name a single Greenhouse job. Open the job on the "
            "employer's site and paste the link from there."));
        return scoutReply;
    }

    const QUrl requestUrl(greenhouseApiBase
                          + QUrl::toPercentEncoding(boardIdentity.tenant)
                          + QStringLiteral("/jobs/")
                          + QUrl::toPercentEncoding(boardIdentity.jobId));

    QNetworkReply *networkReply = networkAccessManager.get(employerBoardRequest(requestUrl));
    networkReply->setParent(scoutReply);

    const QString tenant = boardIdentity.tenant;
    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply, tenant]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        // A job that is gone is not an error. It is the answer, and it is one
        // the user needs: a closed job should stop looking open on the board.
        if (httpStatusCode == 404) {
            scoutReply->markFinished({});
            return;
        }
        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                greenhouseDisplayName, networkReply->errorString(),
                httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                greenhouseDisplayName, httpStatusCode, responseBody));
            return;
        }

        const JobPosting posting = postingFromGreenhouseJob(responseDocument.object(), tenant);
        if (posting.positionTitle.isEmpty()) {
            scoutReply->markFinished({});
            return;
        }
        scoutReply->markFinished({ posting });
    });

    return scoutReply;
}
