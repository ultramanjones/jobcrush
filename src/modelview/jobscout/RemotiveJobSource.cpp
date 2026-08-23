#include "RemotiveJobSource.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"
#include "JobSearchProfile.h"

namespace {

const QString remotiveStorageName = QStringLiteral("remotive");
const QString remotiveJobsEndpointUrl = QStringLiteral("https://remotive.com/api/remote-jobs");

// One sweep's worth. Enough to be genuinely useful, small enough to stay
// polite to a free API that asks nothing of us in return.
constexpr int remotiveMaximumResultsPerSweep = 100;

// What a response actually was, when it turned out not to be job listings.
// Enough to diagnose from — status, size, and the opening bytes — without
// dumping a whole page into a message meant for a human to read.
QString responseDiagnosticTail(int httpStatusCode, const QByteArray &responseBody)
{
    QString openingBytes = QString::fromUtf8(responseBody.left(180)).simplified();
    if (openingBytes.isEmpty()) {
        openingBytes = QStringLiteral("(empty)");
    }
    return QStringLiteral(" [HTTP %1, %2 bytes, starts: %3]")
        .arg(httpStatusCode)
        .arg(responseBody.size())
        .arg(openingBytes);
}

} // namespace

RemotiveJobSource::RemotiveJobSource() = default;

JobSourceDescriptor RemotiveJobSource::descriptor() const
{
    bool found = false;
    return jobSourceDescriptorFor(remotiveStorageName, found);
}

JobScoutReply *RemotiveJobSource::searchForJobs(const JobSearchProfile &searchProfile,
                                                QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    QUrl requestUrl(remotiveJobsEndpointUrl);
    QUrlQuery requestQuery;
    requestQuery.addQueryItem(QStringLiteral("limit"),
                              QString::number(remotiveMaximumResultsPerSweep));

    // Remotive takes ONE search string. The first target title is the user's
    // strongest statement of what they want, so that is what gets asked for;
    // everything else is weighed locally by the scorer.
    const QStringList targetJobTitles = searchProfile.targetJobTitles();
    if (!targetJobTitles.isEmpty()) {
        requestQuery.addQueryItem(QStringLiteral("search"), targetJobTitles.first());
    }
    requestUrl.setQuery(requestQuery);

    QNetworkRequest networkRequest(requestUrl);
    networkRequest.setRawHeader(QByteArrayLiteral("Accept"),
                                QByteArrayLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("User-Agent"),
                                QByteArrayLiteral("Mozilla/5.0 (compatible; JobCrush/0.1; "
                                "+https://github.com/ultramanjones/jobcrush)"));

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
                          .arg(QStringLiteral("Remotive")).arg(httpStatusCode)
                    : QStringLiteral("couldn't reach %1 — %2")
                          .arg(QStringLiteral("Remotive"), networkReply->errorString()));
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
                QStringLiteral("Remotive answered with a web page instead of job data")
                + responseDiagnosticTail(httpStatusCode, responseBody));
            return;
        }

        const QJsonObject responseObject = responseDocument.object();
        const QJsonArray jobsArray = responseObject.value(QStringLiteral("jobs")).toArray();

        // Valid JSON with no listings in it. Not a crash, not a success —
        // and the only way to tell WHY is to describe what did arrive.
        if (jobsArray.isEmpty()) {
            scoutReply->markFailed(
                QStringLiteral("Remotive sent valid data with no jobs in it")
                + responseDiagnosticTail(httpStatusCode, responseBody));
            return;
        }

        const QDateTime sweepTimestamp = QDateTime::currentDateTime();
        QList<JobPosting> foundJobPostings;

        for (const QJsonValue &jobValue : jobsArray) {
            const QJsonObject jobObject = jobValue.toObject();

            JobPosting jobPosting;
            jobPosting.discoverySource = remotiveStorageName;
            jobPosting.externalSourceId =
                QString::number(jobObject.value(QStringLiteral("id")).toVariant().toLongLong());
            jobPosting.positionTitle = jobObject.value(QStringLiteral("title")).toString();
            jobPosting.companyName = jobObject.value(QStringLiteral("company_name")).toString();
            jobPosting.salaryText = jobObject.value(QStringLiteral("salary")).toString();
            jobPosting.sourceUrl = jobObject.value(QStringLiteral("url")).toString();

            // Remotive lists remote roles exclusively; the field it does give
            // is WHERE a remote candidate may live.
            jobPosting.isRemoteRole = true;
            jobPosting.locationText =
                jobObject.value(QStringLiteral("candidate_required_location")).toString();

            jobPosting.fullDescriptionText = plainTextFromHtmlFragment(
                jobObject.value(QStringLiteral("description")).toString());

            jobPosting.postedTimestamp = QDateTime::fromString(
                jobObject.value(QStringLiteral("publication_date")).toString(), Qt::ISODate);
            jobPosting.discoveredTimestamp = sweepTimestamp;

            // A posting with no identity cannot be deduplicated, and one with
            // no link cannot be applied to. Either way it is not useful.
            if (jobPosting.externalSourceId.isEmpty()
                    || jobPosting.externalSourceId == QStringLiteral("0")
                    || jobPosting.sourceUrl.isEmpty()) {
                continue;
            }
            foundJobPostings.append(jobPosting);
        }

        scoutReply->markFinished(foundJobPostings);
    });

    return scoutReply;
}
