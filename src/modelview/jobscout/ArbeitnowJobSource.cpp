#include "ArbeitnowJobSource.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"
#include "JobSearchProfile.h"

namespace {

const QString arbeitnowStorageName = QStringLiteral("arbeitnow");
const QString arbeitnowJobBoardEndpointUrl =
    QStringLiteral("https://www.arbeitnow.com/api/job-board-api");

} // namespace

ArbeitnowJobSource::ArbeitnowJobSource() = default;

JobSourceDescriptor ArbeitnowJobSource::descriptor() const
{
    bool found = false;
    return jobSourceDescriptorFor(arbeitnowStorageName, found);
}

JobScoutReply *ArbeitnowJobSource::searchForJobs(const JobSearchProfile &searchProfile,
                                                 QObject *replyParent)
{
    // The profile shapes nothing here — Arbeitnow accepts no query terms.
    // The scorer earns its keep on this one.
    Q_UNUSED(searchProfile);

    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    QNetworkRequest networkRequest{QUrl(arbeitnowJobBoardEndpointUrl)};
    networkRequest.setRawHeader(QByteArrayLiteral("Accept"),
                                QByteArrayLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("User-Agent"),
                                QByteArrayLiteral("Mozilla/5.0 (compatible; JobCrush/0.1; "
                                "+https://github.com/ultramanjones/jobcrush)"));

    // Follow redirects: a site quietly moving its endpoint should not read to
    // the user as the site being down.
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *networkReply = networkAccessManager.get(networkRequest);
    networkReply->setParent(scoutReply); // dies with the scout reply

    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [networkReply, scoutReply]() {
        // What actually came back, in enough detail to be fixable. A failure
        // that only says "it didn't work" costs the user a support round trip
        // they should never have had to make.
        const int httpStatusCode = networkReply
            ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(
                httpStatusCode > 0
                    ? QStringLiteral("%1 refused the request (HTTP %2)")
                          .arg(QStringLiteral("Arbeitnow")).arg(httpStatusCode)
                    : QStringLiteral("couldn't reach %1 — %2")
                          .arg(QStringLiteral("Arbeitnow"), networkReply->errorString()));
            return;
        }

        QJsonParseError jsonParseError;
        const QJsonDocument responseDocument =
            QJsonDocument::fromJson(responseBody, &jsonParseError);
        if (jsonParseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            // Almost always a bot-check or maintenance page arriving where
            // JSON was expected. Say so plainly instead of showing a parser
            // complaint nobody outside this file can act on.
            scoutReply->markFailed(
                QStringLiteral("%1 answered with a web page instead of job data "
                               "(HTTP %2)").arg(QStringLiteral("Arbeitnow")).arg(httpStatusCode));
            return;
        }

        const QJsonObject responseObject = responseDocument.object();
        const QJsonArray jobsArray = responseObject.value(QStringLiteral("data")).toArray();

        const QDateTime sweepTimestamp = QDateTime::currentDateTime();
        QList<JobPosting> foundJobPostings;

        for (const QJsonValue &jobValue : jobsArray) {
            const QJsonObject jobObject = jobValue.toObject();

            JobPosting jobPosting;
            jobPosting.discoverySource = arbeitnowStorageName;
            // Arbeitnow's slug is its stable per-job identity.
            jobPosting.externalSourceId = jobObject.value(QStringLiteral("slug")).toString();
            jobPosting.positionTitle = jobObject.value(QStringLiteral("title")).toString();
            jobPosting.companyName = jobObject.value(QStringLiteral("company_name")).toString();
            jobPosting.locationText = jobObject.value(QStringLiteral("location")).toString();
            jobPosting.sourceUrl = jobObject.value(QStringLiteral("url")).toString();
            jobPosting.isRemoteRole = jobObject.value(QStringLiteral("remote")).toBool();

            jobPosting.fullDescriptionText = plainTextFromHtmlFragment(
                jobObject.value(QStringLiteral("description")).toString());

            // created_at arrives as Unix seconds rather than a date string.
            const qint64 createdAtUnixSeconds =
                jobObject.value(QStringLiteral("created_at")).toVariant().toLongLong();
            if (createdAtUnixSeconds > 0) {
                jobPosting.postedTimestamp =
                    QDateTime::fromSecsSinceEpoch(createdAtUnixSeconds);
            }
            jobPosting.discoveredTimestamp = sweepTimestamp;

            if (jobPosting.externalSourceId.isEmpty() || jobPosting.sourceUrl.isEmpty()) {
                continue; // nothing to deduplicate on, or nowhere to apply
            }
            foundJobPostings.append(jobPosting);
        }

        scoutReply->markFinished(foundJobPostings);
    });

    return scoutReply;
}
