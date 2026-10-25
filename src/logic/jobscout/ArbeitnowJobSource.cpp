#include "ArbeitnowJobSource.h"

#include <QDebug>
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

// How much of a bad response to quote back in a message.
constexpr int arbeitnowDiagnosticOpeningByteCount = 180;

// What a response actually was, when it turned out not to be job listings.
// Enough to diagnose from — status, size, and the opening bytes — without
// dumping a whole page into a message meant for a human to read.
QString responseDiagnosticTail(int httpStatusCode, const QByteArray &responseBody)
{
    QString openingBytes =
        QString::fromUtf8(responseBody.left(arbeitnowDiagnosticOpeningByteCount)).simplified();
    if (openingBytes.isEmpty()) {
        openingBytes = QStringLiteral("(empty)");
    }
    return QStringLiteral(" [HTTP %1, %2 bytes, starts: %3]")
        .arg(httpStatusCode)
        .arg(responseBody.size())
        .arg(openingBytes);
}

} // namespace

ArbeitnowJobSource::ArbeitnowJobSource() = default;

JobSourceDescriptor ArbeitnowJobSource::descriptor() const
{
    bool descriptorWasFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(arbeitnowStorageName, descriptorWasFound);
    if (!descriptorWasFound) {
        qWarning().noquote() << QStringLiteral(
            "ArbeitnowJobSource::descriptor: no catalog entry for \"%1\" — "
            "the source catalog is missing it")
                .arg(arbeitnowStorageName);
    }
    return descriptor;
}

// Steps: one GET, no query terms. The answer is {"data":[...]}, one object
// per job. Each job is copied into a JobPosting. A job with no slug or no
// link is skipped.
JobScoutReply *ArbeitnowJobSource::searchForJobs(const JobSearchProfile &searchProfile,
                                                 QObject *replyParent)
{
    // The profile shapes nothing here — Arbeitnow accepts no query terms.
    // The local scorer does all of the ranking for this source.
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

        // A status code means the site answered and said no. No status code
        // means the request never got an answer at all.
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
                QStringLiteral("Arbeitnow answered with a web page instead of job data")
                + responseDiagnosticTail(httpStatusCode, responseBody));
            return;
        }

        const QJsonObject responseObject = responseDocument.object();
        const QJsonArray jobsArray = responseObject.value(QStringLiteral("data")).toArray();

        // Valid JSON with no listings in it. Not a crash, not a success —
        // and the only way to tell WHY is to describe what did arrive.
        if (jobsArray.isEmpty()) {
            scoutReply->markFailed(
                QStringLiteral("Arbeitnow sent valid data with no jobs in it")
                + responseDiagnosticTail(httpStatusCode, responseBody));
            return;
        }

        const QDateTime sweepTimestamp = QDateTime::currentDateTime();
        QList<JobPosting> foundJobPostings;

        for (const QJsonValue &jobValue : jobsArray) {
            const QJsonObject jobObject = jobValue.toObject();

            JobPosting jobPosting;
            jobPosting.postingSource = arbeitnowStorageName;
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
