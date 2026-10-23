#include "WorkdayBoardSource.h"

#include <memory>

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"

namespace {

const QString workdayDisplayName = QStringLiteral("Workday");

// Workday refuses a page bigger than 20 with HTTP 400. See trap 1 in the header.
constexpr int workdayPageSize = 20;

// How far a whole-site read goes. 50 pages is 1,000 jobs and 50 requests.
// Past that, reading everything is the wrong tool; a title search is the right one.
constexpr int mostPagesForAWholeSite = 50;

// A title search reads this many pages. Workday ranks loosely (trap 4), so
// the exact title is often not first, but it is almost always near the top.
constexpr int mostPagesForATitleSearch = 3;

// The JSON service lives under /wday/cxs/{tenant}/{site} on the site's host.
QString serviceBaseFor(const QString &hostPrefix, const QString &cxsTenant,
                       const QString &siteName)
{
    return QStringLiteral("https://%1.myworkdayjobs.com/wday/cxs/%2/%3")
        .arg(hostPrefix, cxsTenant, siteName);
}

// The public page for one job, the one a person opens to apply.
QString publicJobUrlFor(const QString &hostPrefix, const QString &siteName,
                        const QString &jobPath)
{
    return QStringLiteral("https://%1.myworkdayjobs.com/%2/%3")
        .arg(hostPrefix, siteName, jobPath);
}

// Workday adds the same request headers as every other board, plus a language,
// because the answer's display text follows it.
QNetworkRequest workdayRequest(const QUrl &requestUrl)
{
    QNetworkRequest networkRequest = employerBoardRequest(requestUrl);
    networkRequest.setRawHeader(QByteArrayLiteral("Accept-Language"),
                                QByteArrayLiteral("en-US"));
    return networkRequest;
}

// True when Workday sent its maintenance page instead of an answer. See trap 5.
bool isWorkdayMaintenancePage(const QNetworkReply *networkReply, const QByteArray &responseBody)
{
    const QString finalHost = networkReply->url().host().toLower();
    if (finalHost.contains(QStringLiteral("community.workday.com"))) {
        return true;
    }
    return responseBody.contains("maintenance-page")
        || responseBody.contains("Workday is currently unavailable");
}

QString maintenanceMessage()
{
    return QStringLiteral("Workday is down for maintenance right now. It usually comes "
                          "back within a few hours. Try again later.");
}

// The job id is the path without its leading slash.
QString jobPathWithoutLeadingSlash(const QString &externalPath)
{
    QString jobPath = externalPath.trimmed();
    while (jobPath.startsWith(QLatin1Char('/'))) {
        jobPath.remove(0, 1);
    }
    return jobPath;
}

bool locationTextSaysRemote(const QString &locationText)
{
    const QString lowered = locationText.toLower();
    return lowered.contains(QStringLiteral("remote"))
        || lowered.contains(QStringLiteral("virtual"))
        || lowered.contains(QStringLiteral("telecommut"));
}

// The state of one search while its pages come in. Held by shared_ptr and
// captured by each page's handler, the same pattern the posting resolver uses.
struct WorkdaySearchInProgress {
    JobScoutReply *scoutReply = nullptr;
    QNetworkAccessManager *networkAccessManager = nullptr;
    QString workdayTenant;
    QString serviceBase;
    QString searchText;
    int mostPagesToRead = 1;
    int pagesRead = 0;
    QList<JobPosting> foundPostings;
};

void readNextPage(const std::shared_ptr<WorkdaySearchInProgress> &search);

void handleOnePage(const std::shared_ptr<WorkdaySearchInProgress> &search,
                   QNetworkReply *networkReply)
{
    const int httpStatusCode =
        networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = networkReply->readAll();

    if (isWorkdayMaintenancePage(networkReply, responseBody)) {
        search->scoutReply->markFailed(maintenanceMessage(), true);
        return;
    }

    // Workday answers 404 for a site name it does not have. That is an answer
    // about the link, not trouble with the connection.
    if (httpStatusCode == 404) {
        search->scoutReply->markFailed(
            noSuchBoardMessage(workdayDisplayName, search->workdayTenant), false);
        return;
    }
    if (networkReply->error() != QNetworkReply::NoError) {
        qWarning().noquote() << QStringLiteral(
            "WorkdayBoardSource::handleOnePage: request failed for \"%1\" page %2 — %3")
            .arg(search->workdayTenant).arg(search->pagesRead + 1)
            .arg(networkReply->errorString());
        search->scoutReply->markFailed(boardDidNotAnswerMessage(
            workdayDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument responseDocument = QJsonDocument::fromJson(responseBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
        qWarning().noquote() << QStringLiteral(
            "WorkdayBoardSource::handleOnePage: unreadable answer for \"%1\"%2")
            .arg(search->workdayTenant,
                 responseDiagnosticTail(httpStatusCode, responseBody));
        search->scoutReply->markFailed(boardSentUnreadableMessage(
            workdayDisplayName, httpStatusCode, responseBody));
        return;
    }

    const QJsonObject answer = responseDocument.object();
    const QJsonArray searchEntries = answer.value(QStringLiteral("jobPostings")).toArray();
    for (const QJsonValue &entryValue : searchEntries) {
        const JobPosting posting = WorkdayBoardSource::postingFromSearchEntry(
            entryValue.toObject(), search->workdayTenant);
        if (posting.positionTitle.isEmpty() || posting.externalSourceId.isEmpty()) {
            continue;
        }
        search->foundPostings.append(posting);
    }
    ++search->pagesRead;

    // Stop when the page came back short, when every job has been read, or at
    // the cap. "total" is sent on every page checked so far, but a short page
    // ends the search even if it is missing.
    const int totalJobs = answer.value(QStringLiteral("total")).toInt(-1);
    const int jobsSeenSoFar = search->pagesRead * workdayPageSize;
    const bool pageWasShort = searchEntries.count() < workdayPageSize;
    const bool everyJobRead = totalJobs >= 0 && jobsSeenSoFar >= totalJobs;
    if (pageWasShort || everyJobRead || search->pagesRead >= search->mostPagesToRead) {
        search->scoutReply->markFinished(search->foundPostings);
        return;
    }
    readNextPage(search);
}

void readNextPage(const std::shared_ptr<WorkdaySearchInProgress> &search)
{
    QJsonObject requestBody;
    requestBody.insert(QStringLiteral("appliedFacets"), QJsonObject());
    requestBody.insert(QStringLiteral("limit"), workdayPageSize);
    requestBody.insert(QStringLiteral("offset"), search->pagesRead * workdayPageSize);
    requestBody.insert(QStringLiteral("searchText"), search->searchText);

    QNetworkRequest networkRequest =
        workdayRequest(QUrl(search->serviceBase + QStringLiteral("/jobs")));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));

    QNetworkReply *networkReply = search->networkAccessManager->post(
        networkRequest, QJsonDocument(requestBody).toJson(QJsonDocument::Compact));
    networkReply->setParent(search->scoutReply);

    QObject::connect(networkReply, &QNetworkReply::finished, search->scoutReply,
                     [search, networkReply]() {
        networkReply->deleteLater();
        handleOnePage(search, networkReply);
    });
}

} // namespace

WorkdayBoardSource::WorkdayBoardSource() = default;

QString WorkdayBoardSource::boardName() const
{
    return AtsBoardName::Workday;
}

bool WorkdayBoardSource::splitTenant(const QString &workdayTenant,
                                     QString &hostPrefix, QString &cxsTenant,
                                     QString &siteName)
{
    // "ngc.wd1/Northrop_Grumman_External_Site"
    static const QRegularExpression tenantPattern(
        QStringLiteral("^([A-Za-z0-9-]+)\\.(wd\\d+)/([A-Za-z0-9_.-]+)$"));
    const QRegularExpressionMatch match = tenantPattern.match(workdayTenant.trimmed());
    if (!match.hasMatch()) {
        return false;
    }
    cxsTenant = match.captured(1);
    hostPrefix = cxsTenant + QLatin1Char('.') + match.captured(2);
    siteName = match.captured(3);
    return true;
}

QString WorkdayBoardSource::companyNameFromSiteName(const QString &siteName)
{
    // Underscores and dashes become spaces, and a run of capitals followed by a
    // capitalized word is split: "L3HarrisCareers" -> "L3Harris Careers".
    QString spaced = siteName;
    spaced.replace(QLatin1Char('_'), QLatin1Char(' '));
    spaced.replace(QLatin1Char('-'), QLatin1Char(' '));
    static const QRegularExpression lowerThenUpperPattern(
        QStringLiteral("([a-z])([A-Z][a-z])"));
    spaced.replace(lowerThenUpperPattern, QStringLiteral("\\1 \\2"));

    // Words that name the careers site, not the company.
    static const QStringList siteWords = {
        QStringLiteral("external"), QStringLiteral("site"), QStringLiteral("careers"),
        QStringLiteral("career"), QStringLiteral("jobs"), QStringLiteral("job"),
        QStringLiteral("en"), QStringLiteral("us"), QStringLiteral("global"),
        QStringLiteral("recruiting"), QStringLiteral("opportunities"),
    };
    QStringList keptWords;
    for (const QString &word : spaced.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (!siteWords.contains(word.toLower())) {
            keptWords.append(word);
        }
    }
    if (keptWords.isEmpty()) {
        return siteName;
    }
    return keptWords.join(QLatin1Char(' '));
}

QDate WorkdayBoardSource::postedDateFromDisplayText(const QString &postedOnText,
                                                    const QDate &today)
{
    const QString lowered = postedOnText.trimmed().toLower();
    if (lowered.endsWith(QStringLiteral("today"))) {
        return today;
    }
    if (lowered.endsWith(QStringLiteral("yesterday"))) {
        return today.addDays(-1);
    }
    // "posted 3 days ago", "posted 30+ days ago". The plus means "at least",
    // so 30 is used; a date that may be too recent beats no date at all.
    static const QRegularExpression daysAgoPattern(
        QStringLiteral("(\\d+)\\+?\\s+days?\\s+ago"));
    const QRegularExpressionMatch match = daysAgoPattern.match(lowered);
    if (match.hasMatch()) {
        return today.addDays(-match.captured(1).toInt());
    }
    return QDate();
}

JobPosting WorkdayBoardSource::postingFromSearchEntry(const QJsonObject &searchEntry,
                                                      const QString &workdayTenant)
{
    JobPosting posting;
    QString hostPrefix;
    QString cxsTenant;
    QString siteName;
    splitTenant(workdayTenant, hostPrefix, cxsTenant, siteName);

    const QString jobPath =
        jobPathWithoutLeadingSlash(searchEntry.value(QStringLiteral("externalPath")).toString());

    posting.positionTitle = searchEntry.value(QStringLiteral("title")).toString().trimmed();
    posting.externalSourceId = jobPath;
    posting.sourceUrl = jobPath.isEmpty() ? QString()
                                          : publicJobUrlFor(hostPrefix, siteName, jobPath);
    posting.companyName = companyNameFromSiteName(siteName);

    // "14 Locations" is what a search row says for a job in many places. It is
    // true but not a place, so it is kept as it is; the one-job answer lists
    // the actual places.
    posting.locationText = searchEntry.value(QStringLiteral("locationsText")).toString().trimmed();
    posting.isRemoteRole = locationTextSaysRemote(posting.locationText);

    const QDate postedDate = postedDateFromDisplayText(
        searchEntry.value(QStringLiteral("postedOn")).toString(), QDate::currentDate());
    if (postedDate.isValid()) {
        posting.postedTimestamp = QDateTime(postedDate, QTime(12, 0));
    }

    posting.postingSource = AtsBoardName::Workday;
    posting.discoveredTimestamp = QDateTime::currentDateTime();
    return posting;
}

JobPosting WorkdayBoardSource::postingFromJobDetail(const QJsonObject &jobDetailAnswer,
                                                    const QString &workdayTenant,
                                                    const QString &jobPath)
{
    JobPosting posting;
    QString hostPrefix;
    QString cxsTenant;
    QString siteName;
    splitTenant(workdayTenant, hostPrefix, cxsTenant, siteName);

    const QJsonObject jobInfo =
        jobDetailAnswer.value(QStringLiteral("jobPostingInfo")).toObject();

    posting.positionTitle = jobInfo.value(QStringLiteral("title")).toString().trimmed();
    posting.externalSourceId = jobPathWithoutLeadingSlash(jobPath);
    posting.companyName = companyNameFromSiteName(siteName);

    // The public link: Workday's own when it sends one, otherwise built from
    // the parts. Both open the same page.
    posting.sourceUrl = jobInfo.value(QStringLiteral("externalUrl")).toString().trimmed();
    if (posting.sourceUrl.isEmpty()) {
        posting.sourceUrl = publicJobUrlFor(hostPrefix, siteName, posting.externalSourceId);
    }

    // The first place, then every other place, on one line.
    QStringList everyPlace;
    const QString firstPlace = jobInfo.value(QStringLiteral("location")).toString().trimmed();
    if (!firstPlace.isEmpty()) {
        everyPlace.append(firstPlace);
    }
    for (const QJsonValue &placeValue :
             jobInfo.value(QStringLiteral("additionalLocations")).toArray()) {
        const QString place = placeValue.toString().trimmed();
        if (!place.isEmpty() && !everyPlace.contains(place)) {
            everyPlace.append(place);
        }
    }
    posting.locationText = everyPlace.join(QStringLiteral("; "));

    const QString remoteTypeText = jobInfo.value(QStringLiteral("remoteType")).toString();
    posting.isRemoteRole = locationTextSaysRemote(posting.locationText)
                        || locationTextSaysRemote(remoteTypeText);

    posting.fullDescriptionText = plainTextFromHtmlFragment(
        jobInfo.value(QStringLiteral("jobDescription")).toString());

    // startDate is a real date. postedOn is display text. Prefer the date.
    const QDate startDate = QDate::fromString(
        jobInfo.value(QStringLiteral("startDate")).toString(), Qt::ISODate);
    if (startDate.isValid()) {
        posting.postedTimestamp = QDateTime(startDate, QTime(12, 0));
    } else {
        const QDate postedDate = postedDateFromDisplayText(
            jobInfo.value(QStringLiteral("postedOn")).toString(), QDate::currentDate());
        if (postedDate.isValid()) {
            posting.postedTimestamp = QDateTime(postedDate, QTime(12, 0));
        }
    }

    posting.postingSource = AtsBoardName::Workday;
    posting.discoveredTimestamp = QDateTime::currentDateTime();
    return posting;
}

JobScoutReply *WorkdayBoardSource::searchSite(const QString &workdayTenant,
                                              const QString &searchText,
                                              int mostPagesToRead,
                                              QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    QString hostPrefix;
    QString cxsTenant;
    QString siteName;
    if (!splitTenant(workdayTenant, hostPrefix, cxsTenant, siteName)) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "Job Crush needs a link to one of the company's Workday jobs before it can "
            "look there. Paste a job link from their careers site."), false);
        return scoutReply;
    }

    auto search = std::make_shared<WorkdaySearchInProgress>();
    search->scoutReply = scoutReply;
    search->networkAccessManager = &networkAccessManager;
    search->workdayTenant = workdayTenant.trimmed();
    search->serviceBase = serviceBaseFor(hostPrefix, cxsTenant, siteName);
    search->searchText = searchText.trimmed();
    search->mostPagesToRead = mostPagesToRead;
    readNextPage(search);
    return scoutReply;
}

JobScoutReply *WorkdayBoardSource::fetchEveryJobForEmployer(const QString &tenant,
                                                            QObject *replyParent)
{
    return searchSite(tenant, QString(), mostPagesForAWholeSite, replyParent);
}

JobScoutReply *WorkdayBoardSource::fetchJobsLikeTitle(const QString &tenant,
                                                      const QString &positionTitle,
                                                      QObject *replyParent)
{
    return searchSite(tenant, positionTitle, mostPagesForATitleSearch, replyParent);
}

JobScoutReply *WorkdayBoardSource::fetchOneJob(const AtsBoardIdentity &boardIdentity,
                                               QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    QString hostPrefix;
    QString cxsTenant;
    QString siteName;
    if (!boardIdentity.namesOneJob()
            || !splitTenant(boardIdentity.tenant, hostPrefix, cxsTenant, siteName)) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "That link doesn't name a single Workday job. Open the job on the "
            "employer's careers site and paste the link from there."), false);
        return scoutReply;
    }

    const QString jobPath = jobPathWithoutLeadingSlash(boardIdentity.jobId);
    const QUrl requestUrl(serviceBaseFor(hostPrefix, cxsTenant, siteName)
                          + QLatin1Char('/') + jobPath);

    QNetworkReply *networkReply = networkAccessManager.get(workdayRequest(requestUrl));
    networkReply->setParent(scoutReply);

    const QString workdayTenant = boardIdentity.tenant;
    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply, workdayTenant, jobPath]() {
        networkReply->deleteLater();
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (isWorkdayMaintenancePage(networkReply, responseBody)) {
            scoutReply->markFailed(maintenanceMessage(), true);
            return;
        }

        // A job that is gone is an answer, not an error. Workday says 404.
        if (httpStatusCode == 404) {
            scoutReply->markFinished({});
            return;
        }
        if (networkReply->error() != QNetworkReply::NoError) {
            qWarning().noquote() << QStringLiteral(
                "WorkdayBoardSource::fetchOneJob: request failed for \"%1\" job \"%2\" — %3")
                .arg(workdayTenant, jobPath, networkReply->errorString());
            scoutReply->markFailed(boardDidNotAnswerMessage(
                workdayDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            qWarning().noquote() << QStringLiteral(
                "WorkdayBoardSource::fetchOneJob: unreadable answer for \"%1\" job \"%2\"%3")
                .arg(workdayTenant, jobPath,
                     responseDiagnosticTail(httpStatusCode, responseBody));
            scoutReply->markFailed(boardSentUnreadableMessage(
                workdayDisplayName, httpStatusCode, responseBody));
            return;
        }

        const JobPosting posting = WorkdayBoardSource::postingFromJobDetail(
            responseDocument.object(), workdayTenant, jobPath);
        if (posting.positionTitle.isEmpty()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                workdayDisplayName, httpStatusCode, responseBody));
            return;
        }
        scoutReply->markFinished({ posting });
    });

    return scoutReply;
}
