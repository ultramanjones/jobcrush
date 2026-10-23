#include "SmartRecruitersBoardSource.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <functional>
#include <memory>

#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"

namespace {

const QString smartRecruitersApiBase =
    QStringLiteral("https://api.smartrecruiters.com/v1/companies/");
const QString smartRecruitersDisplayName = QStringLiteral("SmartRecruiters");
constexpr int jobsPerPage = 100;

QString encoded(const QString &text)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(text));
}

QUrl pageUrl(const QString &tenant, const QString &searchWords, int offset)
{
    QUrl url(smartRecruitersApiBase + encoded(tenant) + QStringLiteral("/postings"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"), QString::number(jobsPerPage));
    query.addQueryItem(QStringLiteral("offset"), QString::number(offset));
    if (!searchWords.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("q"), searchWords.trimmed());
    }
    url.setQuery(query);
    return url;
}

QUrl oneJobUrl(const QString &tenant, const QString &jobId)
{
    return QUrl(smartRecruitersApiBase + encoded(tenant) + QStringLiteral("/postings/")
                + encoded(jobId));
}

// "Miskolc, , Hungary" happens when a job has no region. The empty part is
// taken out.
QString cleanLocationText(const QString &fullLocation)
{
    QStringList parts;
    for (const QString &part : fullLocation.split(QLatin1Char(','))) {
        const QString trimmedPart = part.trimmed();
        if (!trimmedPart.isEmpty()) {
            parts.append(trimmedPart);
        }
    }
    return parts.join(QStringLiteral(", "));
}

// The fields both the list and the one-job answer share.
JobPosting postingFromSmartRecruitersJob(const QJsonObject &jobObject, const QString &tenant)
{
    JobPosting posting;
    posting.externalSourceId = jobObject.value(QStringLiteral("id")).toString().trimmed();
    posting.positionTitle = jobObject.value(QStringLiteral("name")).toString().trimmed();

    const QJsonObject companyObject = jobObject.value(QStringLiteral("company")).toObject();
    posting.companyName = companyObject.value(QStringLiteral("name")).toString().trimmed();
    if (posting.companyName.isEmpty()) {
        posting.companyName = companyNameFromTenant(tenant);
    }
    const QString companyIdentifier =
        companyObject.value(QStringLiteral("identifier")).toString().trimmed();

    const QJsonObject locationObject = jobObject.value(QStringLiteral("location")).toObject();
    posting.locationText =
        cleanLocationText(locationObject.value(QStringLiteral("fullLocation")).toString());
    posting.isRemoteRole = locationObject.value(QStringLiteral("remote")).toBool();
    if (posting.isRemoteRole && !posting.locationText.contains(QStringLiteral("remote"),
                                                               Qt::CaseInsensitive)) {
        posting.locationText = posting.locationText.isEmpty()
            ? QStringLiteral("Remote")
            : posting.locationText + QStringLiteral(" (remote)");
    }

    posting.postedTimestamp = QDateTime::fromString(
        jobObject.value(QStringLiteral("releasedDate")).toString(), Qt::ISODateWithMs);

    // Only the one-job answer carries the public link. For a list item the
    // link is built the way SmartRecruiters' own links are: company, then id.
    // AtsBoardDetector reads the id back out of it.
    posting.sourceUrl = jobObject.value(QStringLiteral("postingUrl")).toString().trimmed();
    if (posting.sourceUrl.isEmpty() && !posting.externalSourceId.isEmpty()) {
        posting.sourceUrl = QStringLiteral("https://jobs.smartrecruiters.com/%1/%2")
                                .arg(companyIdentifier.isEmpty() ? tenant : companyIdentifier,
                                     posting.externalSourceId);
    }

    // The description, in the one-job answer only.
    const QJsonObject sections = jobObject.value(QStringLiteral("jobAd")).toObject()
        .value(QStringLiteral("sections")).toObject();
    QStringList descriptionParts;
    for (const QString &sectionName : { QStringLiteral("jobDescription"),
                                        QStringLiteral("qualifications"),
                                        QStringLiteral("additionalInformation"),
                                        QStringLiteral("companyDescription") }) {
        const QJsonObject section = sections.value(sectionName).toObject();
        const QString sectionText =
            plainTextFromHtmlFragment(section.value(QStringLiteral("text")).toString());
        if (sectionText.isEmpty()) {
            continue;
        }
        const QString sectionTitle = section.value(QStringLiteral("title")).toString().trimmed();
        if (!sectionTitle.isEmpty()) {
            descriptionParts.append(sectionTitle);
        }
        descriptionParts.append(sectionText);
    }
    posting.fullDescriptionText = descriptionParts.join(QStringLiteral("\n\n"));

    posting.postingSource = AtsBoardName::SmartRecruiters;
    posting.discoveredTimestamp = QDateTime::currentDateTime();
    return posting;
}

// One read of a board, page after page.
struct PagedRead {
    JobScoutReply *scoutReply = nullptr;
    QString tenant;
    QString searchWords;
    int pagesAllowed = 1;
    int pagesRead = 0;
    QList<JobPosting> found;
};

} // namespace

SmartRecruitersBoardSource::SmartRecruitersBoardSource() = default;

JobPosting SmartRecruitersBoardSource::postingFromJob(const QJsonObject &jobObject,
                                                      const QString &tenant)
{
    return postingFromSmartRecruitersJob(jobObject, tenant);
}

QString SmartRecruitersBoardSource::boardName() const
{
    return AtsBoardName::SmartRecruiters;
}

JobScoutReply *SmartRecruitersBoardSource::fetchEveryJobForEmployer(const QString &tenant,
                                                                    QObject *replyParent)
{
    return fetchPages(tenant, QString(), maxPagesPerBoard, replyParent);
}

JobScoutReply *SmartRecruitersBoardSource::fetchJobsLikeTitle(const QString &tenant,
                                                              const QString &positionTitle,
                                                              QObject *replyParent)
{
    // Its own search, one page. The resolver picks the right title from it.
    return fetchPages(tenant, positionTitle, 1, replyParent);
}

JobScoutReply *SmartRecruitersBoardSource::fetchPages(const QString &tenant,
                                                      const QString &searchWords,
                                                      int pagesAllowed,
                                                      QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    const QString trimmedTenant = tenant.trimmed();
    if (trimmedTenant.isEmpty()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "Job Crush needs the company's SmartRecruiters name before it can look. Paste "
            "a link to one of their jobs and it will work the name out."));
        return scoutReply;
    }

    auto pagedRead = std::make_shared<PagedRead>();
    pagedRead->scoutReply = scoutReply;
    pagedRead->tenant = trimmedTenant;
    pagedRead->searchWords = searchWords;
    pagedRead->pagesAllowed = pagesAllowed;

    // Reads one page, then asks for the next until the board runs out or the
    // page limit is reached.
    auto readNextPage = std::make_shared<std::function<void()>>();
    *readNextPage = [this, pagedRead, readNextPage]() {
        QNetworkReply *networkReply = networkAccessManager.get(employerBoardRequest(
            pageUrl(pagedRead->tenant, pagedRead->searchWords,
                    pagedRead->pagesRead * jobsPerPage)));
        networkReply->setParent(pagedRead->scoutReply);

        QObject::connect(networkReply, &QNetworkReply::finished, pagedRead->scoutReply,
                         [pagedRead, readNextPage, networkReply]() {
            const int httpStatusCode =
                networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray responseBody = networkReply->readAll();

            if (networkReply->error() != QNetworkReply::NoError) {
                pagedRead->scoutReply->markFailed(boardDidNotAnswerMessage(
                    smartRecruitersDisplayName, networkReply->errorString(),
                    httpStatusCode, responseBody));
                *readNextPage = nullptr; // breaks the capture loop
                return;
            }

            QJsonParseError parseError;
            const QJsonDocument responseDocument =
                QJsonDocument::fromJson(responseBody, &parseError);
            if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
                pagedRead->scoutReply->markFailed(boardSentUnreadableMessage(
                    smartRecruitersDisplayName, httpStatusCode, responseBody));
                *readNextPage = nullptr;
                return;
            }

            const QJsonObject pageObject = responseDocument.object();
            const QJsonArray jobs = pageObject.value(QStringLiteral("content")).toArray();
            for (const QJsonValue &jobValue : jobs) {
                const JobPosting posting =
                    postingFromSmartRecruitersJob(jobValue.toObject(), pagedRead->tenant);
                if (posting.positionTitle.isEmpty() || posting.externalSourceId.isEmpty()) {
                    continue;
                }
                pagedRead->found.append(posting);
            }
            ++pagedRead->pagesRead;

            const int totalFound = pageObject.value(QStringLiteral("totalFound")).toInt();
            const int readSoFar = pagedRead->pagesRead * jobsPerPage;
            const bool moreToRead = !jobs.isEmpty() && readSoFar < totalFound;
            if (moreToRead && pagedRead->pagesRead < pagedRead->pagesAllowed) {
                (*readNextPage)();
                return;
            }

            // Stopped before the end: say so, so no job past the stop is
            // ever taken for gone.
            pagedRead->scoutReply->setReadOnlyPartOfTheBoard(moreToRead
                                                             || !pagedRead->searchWords.isEmpty());
            pagedRead->scoutReply->markFinished(pagedRead->found);
            *readNextPage = nullptr;
        });
    };
    (*readNextPage)();

    return scoutReply;
}

JobScoutReply *SmartRecruitersBoardSource::fetchOneJob(const AtsBoardIdentity &boardIdentity,
                                                       QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    if (!boardIdentity.namesOneJob()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "That link doesn't name a single SmartRecruiters job. Open the job on the "
            "employer's site and paste the link from there."));
        return scoutReply;
    }

    QNetworkReply *networkReply = networkAccessManager.get(employerBoardRequest(
        oneJobUrl(boardIdentity.tenant, boardIdentity.jobId)));
    networkReply->setParent(scoutReply);

    const QString tenant = boardIdentity.tenant;
    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply, tenant]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        // A job that is gone is an answer, not an error.
        if (httpStatusCode == 404 || httpStatusCode == 410) {
            scoutReply->markFinished({});
            return;
        }
        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                smartRecruitersDisplayName, networkReply->errorString(), httpStatusCode,
                responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument =
            QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                smartRecruitersDisplayName, httpStatusCode, responseBody));
            return;
        }

        const QJsonObject jobObject = responseDocument.object();
        // The answer says outright when a job is no longer open.
        if (jobObject.contains(QStringLiteral("active"))
                && !jobObject.value(QStringLiteral("active")).toBool()) {
            scoutReply->markFinished({});
            return;
        }
        const JobPosting posting = postingFromSmartRecruitersJob(jobObject, tenant);
        if (posting.positionTitle.isEmpty()) {
            scoutReply->markFinished({});
            return;
        }
        scoutReply->markFinished({ posting });
    });

    return scoutReply;
}
