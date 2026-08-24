#include "AshbyBoardSource.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QStringList>
#include <QUrl>

#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"

namespace {

const QString ashbyApiBase =
    QStringLiteral("https://api.ashbyhq.com/posting-api/job-board/");
const QString ashbyDisplayName = QStringLiteral("Ashby");

QUrl ashbyBoardUrl(const QString &tenant)
{
    return QUrl(ashbyApiBase + QString::fromUtf8(QUrl::toPercentEncoding(tenant))
                + QStringLiteral("?includeCompensation=true"));
}

// Where the job is. Ashby names one main place and lists the rest separately.
// A job open in three cities should say all three.
QString locationTextFrom(const QJsonObject &jobObject)
{
    QStringList placeNames;

    const QString mainLocation =
        jobObject.value(QStringLiteral("location")).toString().trimmed();
    if (!mainLocation.isEmpty()) {
        placeNames.append(mainLocation);
    }

    const QJsonArray otherLocations =
        jobObject.value(QStringLiteral("secondaryLocations")).toArray();
    for (const QJsonValue &locationValue : otherLocations) {
        const QString placeName = locationValue.toObject()
            .value(QStringLiteral("location")).toString().trimmed();
        if (!placeName.isEmpty() && !placeNames.contains(placeName)) {
            placeNames.append(placeName);
        }
    }

    return placeNames.join(QStringLiteral(", "));
}

// Pay, but only when the employer chose to publish it.
//
// Ashby sends compensation data to the board even for employers who have the
// setting turned off, and honoring that switch is the whole point of it. Job
// Crush does not show a number the employer decided not to show.
QString salaryTextFrom(const QJsonObject &jobObject)
{
    if (!jobObject.value(QStringLiteral("shouldDisplayCompensationOnJobPostings")).toBool()) {
        return QString();
    }

    const QJsonObject compensationObject =
        jobObject.value(QStringLiteral("compensation")).toObject();

    // The summary line is already written for people to read: "€110K – €185K
    // • Offers Equity". Rebuilding that out of the tier objects would produce
    // a worse version of a string Ashby already got right.
    const QString summaryLine = compensationObject
        .value(QStringLiteral("compensationTierSummary")).toString().trimmed();
    if (!summaryLine.isEmpty()) {
        return summaryLine;
    }
    return compensationObject
        .value(QStringLiteral("scrapeableCompensationSalarySummary")).toString().trimmed();
}

JobPosting postingFromAshbyJob(const QJsonObject &jobObject, const QString &tenant)
{
    JobPosting posting;

    posting.externalSourceId = jobObject.value(QStringLiteral("id")).toString().trimmed();
    posting.positionTitle = jobObject.value(QStringLiteral("title")).toString().trimmed();
    posting.companyName = companyNameFromTenant(tenant);

    // jobUrl is the posting. applyUrl is the form. The user reads before
    // applying, so the posting is the link and the form is the fallback.
    posting.sourceUrl = jobObject.value(QStringLiteral("jobUrl")).toString().trimmed();
    if (posting.sourceUrl.isEmpty()) {
        posting.sourceUrl = jobObject.value(QStringLiteral("applyUrl")).toString().trimmed();
    }

    posting.locationText = locationTextFrom(jobObject);
    posting.salaryText = salaryTextFrom(jobObject);

    // Ashby sends the same words twice, plain and as HTML. Take the plain one
    // and only fall back to stripping the markup, because a board's own plain
    // version keeps the line breaks the tag stripper has to guess at.
    posting.fullDescriptionText =
        jobObject.value(QStringLiteral("descriptionPlain")).toString().trimmed();
    if (posting.fullDescriptionText.isEmpty()) {
        posting.fullDescriptionText = plainTextFromHtmlFragment(
            jobObject.value(QStringLiteral("descriptionHtml")).toString());
    }

    const QString publishedText =
        jobObject.value(QStringLiteral("publishedAt")).toString().trimmed();
    if (!publishedText.isEmpty()) {
        posting.postedTimestamp = QDateTime::fromString(publishedText, Qt::ISODate);
    }

    // Ashby answers this outright in two fields. Believe them, and only read
    // the location words when both are absent.
    const QString workplaceType =
        jobObject.value(QStringLiteral("workplaceType")).toString().toLower();
    const QString locationLowered = posting.locationText.toLower();
    posting.isRemoteRole = jobObject.value(QStringLiteral("isRemote")).toBool()
                        || workplaceType == QStringLiteral("remote")
                        || locationLowered.contains(QStringLiteral("remote"))
                        || locationLowered.contains(QStringLiteral("anywhere"))
                        || locationLowered.contains(QStringLiteral("distributed"));

    posting.discoverySource = AtsBoardName::Ashby;
    posting.discoveredTimestamp = QDateTime::currentDateTime();
    return posting;
}

} // namespace

AshbyBoardSource::AshbyBoardSource() = default;

QString AshbyBoardSource::boardName() const
{
    return AtsBoardName::Ashby;
}

JobScoutReply *AshbyBoardSource::fetchEveryJobForEmployer(const QString &tenant,
                                                          QObject *replyParent)
{
    return fetchBoardAndKeep(tenant, QString(), replyParent);
}

JobScoutReply *AshbyBoardSource::fetchOneJob(const AtsBoardIdentity &boardIdentity,
                                             QObject *replyParent)
{
    if (!boardIdentity.namesOneJob()) {
        JobScoutReply *scoutReply = new JobScoutReply(replyParent);
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "That link doesn't name a single Ashby job. Open the job on the employer's "
            "site and paste the link from there."));
        return scoutReply;
    }
    return fetchBoardAndKeep(boardIdentity.tenant, boardIdentity.jobId, replyParent);
}

JobScoutReply *AshbyBoardSource::fetchBoardAndKeep(const QString &tenant,
                                                   const QString &wantedJobId,
                                                   QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    const QString trimmedTenant = tenant.trimmed();
    if (trimmedTenant.isEmpty()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "Job Crush needs the company's Ashby name before it can look. Paste a link "
            "to one of their jobs and it will work the name out."));
        return scoutReply;
    }

    QNetworkReply *networkReply =
        networkAccessManager.get(employerBoardRequest(ashbyBoardUrl(trimmedTenant)));
    networkReply->setParent(scoutReply); // dies with the scout reply

    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply, trimmedTenant, wantedJobId]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (httpStatusCode == 404) {
            // Asking for one job off a board that does not exist is a spelling
            // problem either way, so it gets the same answer.
            scoutReply->markFailed(noSuchBoardMessage(ashbyDisplayName, trimmedTenant), false);
            return;
        }
        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                ashbyDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument =
            QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                ashbyDisplayName, httpStatusCode, responseBody));
            return;
        }

        const QJsonArray jobArray =
            responseDocument.object().value(QStringLiteral("jobs")).toArray();

        QList<JobPosting> foundPostings;
        for (const QJsonValue &jobValue : jobArray) {
            const QJsonObject jobObject = jobValue.toObject();

            // isListed false means the employer pulled it off their public
            // board. It is not open to apply to, so it is not a find.
            if (!jobObject.value(QStringLiteral("isListed")).toBool()) {
                continue;
            }

            const JobPosting posting = postingFromAshbyJob(jobObject, trimmedTenant);

            // No id means Job Crush cannot recognize it on the next sweep, so
            // it would be stored again every single time.
            if (posting.positionTitle.isEmpty() || posting.sourceUrl.isEmpty()
                    || posting.externalSourceId.isEmpty()) {
                continue;
            }
            if (!wantedJobId.isEmpty() && posting.externalSourceId != wantedJobId) {
                continue;
            }
            foundPostings.append(posting);
        }

        // When one job was asked for and the board no longer lists it, an
        // empty result IS the answer: that job has closed.
        scoutReply->markFinished(foundPostings);
    });

    return scoutReply;
}
