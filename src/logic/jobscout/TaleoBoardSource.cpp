#include "TaleoBoardSource.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLocale>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include "AtsBoardIdentity.h"
#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"

namespace {

const QString taleoDisplayName = QStringLiteral("Taleo");

// A title search reads this many pages of 25. The caller picks the right
// title out of them.
constexpr int pagesForATitleSearch = 2;

// Looking up one job by its number finds one job or none, so one page.
constexpr int pagesForOneJobLookup = 1;

// Account names this short are usually initials, so they are written in
// capitals: "hdr" becomes "HDR".
constexpr int longestAccountNameTreatedAsInitials = 4;

// The marker at the front of a long text value on a job page.
const QString taleoLongTextMarker = QStringLiteral("!*!");

// Taleo writes a place as country-region-city, three parts.
constexpr int partsInATaleoPlace = 3;

constexpr int secondsPerMinute = 60;
constexpr int minutesPerHour = 60;

// Taleo's dates have no time of day. Noon is used so that moving the time a
// few hours for a time zone does not land it on a different date.
const QTime timeOfDayForADateOnlyPost(12, 0);

// https://{account}.taleo.net
QString siteRootFor(const QString &accountName)
{
    return QStringLiteral("https://%1.taleo.net").arg(accountName);
}

// The careers section's search page, which holds the portal number.
QUrl searchPageUrlFor(const QString &accountName, const QString &careerSectionName)
{
    return QUrl(QStringLiteral("%1/careersection/%2/jobsearch.ftl?lang=en")
                    .arg(siteRootFor(accountName), careerSectionName));
}

// The public page for one job, the one a person opens to apply.
QString jobPageUrlFor(const QString &accountName, const QString &careerSectionName,
                      const QString &jobNumber)
{
    return QStringLiteral("%1/careersection/%2/jobdetail.ftl?job=%3&lang=en")
        .arg(siteRootFor(accountName), careerSectionName, jobNumber);
}

// The JSON search service:
// https://{account}.taleo.net/careersection/rest/jobboard/searchjobs?lang=en&portal={portal}
QUrl searchServiceUrlFor(const QString &accountName, const QString &portalNumber)
{
    QUrl searchUrl(QStringLiteral("%1/careersection/rest/jobboard/searchjobs")
                       .arg(siteRootFor(accountName)));
    QUrlQuery searchQuery;
    searchQuery.addQueryItem(QStringLiteral("lang"), QStringLiteral("en"));
    searchQuery.addQueryItem(QStringLiteral("portal"), portalNumber);
    searchUrl.setQuery(searchQuery);
    return searchUrl;
}

// The body the careers page itself sends. searchWords may be a job number,
// which finds exactly that job (see the header, point 4).
//
// Only two values change: KEYWORD (the search words) and pageNo (counted
// from 1). Every other field is copied as the careers page sends it, with
// no filters set. sortBySelectionParam "3" is the page's own sort choice;
// Job Crush keeps it as seen and does not rely on what it sorts by.
QByteArray searchRequestBody(const QString &searchWords, int pageNumber)
{
    const QJsonObject requestBody {
        { QStringLiteral("multilineEnabled"), false },
        { QStringLiteral("sortingSelection"), QJsonObject {
              { QStringLiteral("sortBySelectionParam"), QStringLiteral("3") },
              { QStringLiteral("ascendingSortingOrder"), QStringLiteral("false") } } },
        { QStringLiteral("fieldData"), QJsonObject {
              { QStringLiteral("fields"), QJsonObject {
                    { QStringLiteral("KEYWORD"), searchWords },
                    { QStringLiteral("LOCATION"), QString() } } },
              { QStringLiteral("valid"), true } } },
        { QStringLiteral("filterSelectionParam"), QJsonObject {
              { QStringLiteral("searchFilterSelections"), QJsonArray() } } },
        { QStringLiteral("advancedSearchFiltersSelectionParam"), QJsonObject {
              { QStringLiteral("searchFilterSelections"), QJsonArray() } } },
        { QStringLiteral("pageNo"), pageNumber },
    };
    return QJsonDocument(requestBody).toJson(QJsonDocument::Compact);
}

// Taleo's search wants the caller's time zone in a "tz" header, written like
// "GMT-04:00". It is only used for how dates are shown.
//
// Steps: take this computer's offset from UTC in seconds. The sign is kept
// apart, and the size is turned into whole minutes. Hours are minutes / 60
// and the rest is minutes % 60. Each is written as two digits with a
// leading zero.
QByteArray timeZoneHeaderValue()
{
    const int offsetSeconds = QDateTime::currentDateTime().offsetFromUtc();
    const int offsetMinutes = qAbs(offsetSeconds) / secondsPerMinute;
    return QStringLiteral("GMT%1%2:%3")
        .arg(offsetSeconds < 0 ? QStringLiteral("-") : QStringLiteral("+"))
        .arg(offsetMinutes / minutesPerHour, 2, 10, QLatin1Char('0'))
        .arg(offsetMinutes % minutesPerHour, 2, 10, QLatin1Char('0'))
        .toLatin1();
}

// A request for an HTML page (the search page or a job page), not JSON.
QNetworkRequest pageRequest(const QUrl &requestUrl)
{
    QNetworkRequest networkRequest = employerBoardRequest(requestUrl);
    networkRequest.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("text/html"));
    return networkRequest;
}

// "hdr" -> "HDR", "acmerobotics" -> "Acmerobotics". A stand-in for when
// nobody told Job Crush the company's name. Short account names are usually
// initials.
QString companyNameFromAccountName(const QString &accountName)
{
    if (accountName.length() <= longestAccountNameTreatedAsInitials) {
        return accountName.toUpper();
    }
    return accountName.left(1).toUpper() + accountName.mid(1);
}

// Taleo writes a place as "United States-Ohio-Columbus". Turned around to
// "Columbus, Ohio, United States", the way people read it. Anything that is
// not three parts, like "Multiple Locations", is kept as it is.
QString readablePlaceText(const QString &taleoPlace)
{
    const QStringList placeParts = taleoPlace.split(QLatin1Char('-'));
    if (placeParts.count() != partsInATaleoPlace) {
        return taleoPlace.trimmed();
    }
    return QStringLiteral("%1, %2, %3").arg(placeParts.at(2).trimmed(),
                                            placeParts.at(1).trimmed(),
                                            placeParts.at(0).trimmed());
}

// Undoes the escaping inside a single-quoted JavaScript string.
//
// Steps: walk the text one character at a time. A backslash with a
// character after it is an escape: "\n" becomes a line break, and any other
// escaped character ("\'", "\"", "\\") is kept as itself without the
// backslash. Every other character is copied as it is. A lone backslash at
// the very end is copied as it is.
QString unescapedJavaScriptText(const QString &escapedText)
{
    QString plainText;
    plainText.reserve(escapedText.size());
    for (int characterIndex = 0; characterIndex < escapedText.size(); ++characterIndex) {
        const QChar character = escapedText.at(characterIndex);
        if (character == QLatin1Char('\\') && characterIndex + 1 < escapedText.size()) {
            ++characterIndex;
            const QChar escapedCharacter = escapedText.at(characterIndex);
            if (escapedCharacter == QLatin1Char('n')) {
                plainText += QLatin1Char('\n');
            } else {
                plainText += escapedCharacter; // \' \" \\ and the rest
            }
            continue;
        }
        plainText += character;
    }
    return plainText;
}

// Every single-quoted string in a piece of JavaScript, unescaped, in order.
//
// The pattern reads: a single quote, then group 1, then a closing single
// quote. Group 1 is any run of pieces, where each piece is either one
// character that is not a quote or a backslash ([^'\\]), or a backslash and
// whatever character follows it (\\.). That second kind is what lets an
// escaped quote, \', sit inside the string without ending it. globalMatch
// walks every such string from left to right.
QStringList quotedStringsIn(const QString &javaScriptText)
{
    static const QRegularExpression quotedStringPattern(
        QStringLiteral("'((?:[^'\\\\]|\\\\.)*)'"));
    QStringList quotedStrings;
    QRegularExpressionMatchIterator matches = quotedStringPattern.globalMatch(javaScriptText);
    while (matches.hasNext()) {
        quotedStrings.append(unescapedJavaScriptText(matches.next().captured(1)));
    }
    return quotedStrings;
}

// A long text value from a job page: "!*!" and then URL-encoded HTML.
// Steps: drop the marker, undo the %XX encoding, then strip the HTML.
QString plainTextFromTaleoValue(const QString &taleoValue)
{
    QString encodedText = taleoValue;
    if (encodedText.startsWith(taleoLongTextMarker)) {
        encodedText.remove(0, taleoLongTextMarker.size());
    }
    const QString htmlText = QUrl::fromPercentEncoding(encodedText.toUtf8());
    return plainTextFromHtmlFragment(htmlText);
}

} // namespace

// What one search is doing: which page it is on, and what it has found.
struct TaleoBoardSource::TaleoSearch {
    JobScoutReply *scoutReply = nullptr;
    QString taleoTenant;
    QString accountName;
    QString portalNumber;
    QString searchWords;
    int pagesAllowed = 1;
    int pagesRead = 0;
    QList<JobPosting> foundPostings;
};

TaleoBoardSource::TaleoBoardSource() = default;

QString TaleoBoardSource::boardName() const
{
    return AtsBoardName::Taleo;
}

bool TaleoBoardSource::readTenant(const QString &taleoTenant, QString &accountName,
                                  QString &careerSectionName)
{
    // The pattern reads, start to end: group 1, the account (letters, digits,
    // dashes), a slash, then group 2, the careers section (letters, digits,
    // _ . -). Nothing else is allowed before, between or after.
    static const QRegularExpression tenantPattern(
        QStringLiteral("^([A-Za-z0-9-]+)/([A-Za-z0-9_.-]+)$"));
    const QRegularExpressionMatch match = tenantPattern.match(taleoTenant.trimmed());
    if (!match.hasMatch()) {
        return false;
    }
    accountName = match.captured(1);
    careerSectionName = match.captured(2);
    return true;
}

QString TaleoBoardSource::portalNumberFromSearchPage(const QString &searchPageHtml)
{
    // The page's own settings first, then any link that carries the number.
    //
    // First pattern: "portalNo", optional spaces, a colon, optional spaces,
    // an optional quote, then the digits as group 1. That matches
    // portalNo: '12345' and portalNo:"12345".
    // Second pattern: "portal=" and the digits after it, as in a link.
    static const QRegularExpression portalSettingPattern(
        QStringLiteral("portalNo\\s*:\\s*['\"]?(\\d+)"));
    static const QRegularExpression portalLinkPattern(QStringLiteral("portal=(\\d+)"));

    const QRegularExpressionMatch settingMatch = portalSettingPattern.match(searchPageHtml);
    if (settingMatch.hasMatch()) {
        return settingMatch.captured(1);
    }
    const QRegularExpressionMatch linkMatch = portalLinkPattern.match(searchPageHtml);
    return linkMatch.hasMatch() ? linkMatch.captured(1) : QString();
}

QDate TaleoBoardSource::postedDateFromColumnText(const QString &columnText)
{
    // The forms seen on Taleo sites: "Oct 23, 2026" and "27/Oct/2026". The
    // month names are English, so a fixed US English locale reads them on
    // every computer, whatever its own language.
    const QLocale englishLocale(QLocale::English, QLocale::UnitedStates);
    // The date part alone, without a time after it: "Oct 23, 2026, 6:16:00 AM"
    // and "27/Oct/2026, 7:59:00 PM" both have one.
    //
    // Three tries, in order: everything before the second comma (fits
    // "Oct 23, 2026, ..."), everything before the first comma (fits
    // "27/Oct/2026, ..."), then the whole text. Each try is read in both
    // date forms. The first one that makes a real date wins.
    const QStringList candidateTexts = {
        columnText.section(QLatin1Char(','), 0, 1).trimmed(),
        columnText.section(QLatin1Char(','), 0, 0).trimmed(),
        columnText.trimmed(),
    };
    for (const QString &candidateText : candidateTexts) {
        for (const QString &dateFormat : { QStringLiteral("MMM d, yyyy"),
                                           QStringLiteral("d/MMM/yyyy") }) {
            const QDate parsedDate = englishLocale.toDate(candidateText, dateFormat);
            if (parsedDate.isValid()) {
                return parsedDate;
            }
        }
    }
    return QDate();
}

JobPosting TaleoBoardSource::postingFromRequisition(const QJsonObject &requisitionObject,
                                                   const QString &taleoTenant)
{
    JobPosting posting;
    QString accountName;
    QString careerSectionName;
    if (!readTenant(taleoTenant, accountName, careerSectionName)) {
        qWarning().noquote() << QStringLiteral(
            "TaleoBoardSource::postingFromRequisition: could not read tenant \"%1\" — "
            "it is not in the form account/section, so the job is skipped")
                .arg(taleoTenant);
        return posting;
    }

    // Which column is which comes from the job itself (header, point 3).
    // linkedColumn is the title's position. locationsColumns is a list of
    // positions, and the first is the one with the places. -1 means no
    // place column.
    const QJsonArray columns = requisitionObject.value(QStringLiteral("column")).toArray();
    const int titleColumn = requisitionObject.value(QStringLiteral("linkedColumn")).toInt(0);
    const QJsonArray locationColumns =
        requisitionObject.value(QStringLiteral("locationsColumns")).toArray();
    const int placeColumn = locationColumns.isEmpty() ? -1 : locationColumns.first().toInt();

    const QString jobNumber = requisitionObject.value(QStringLiteral("contestNo")).toString();
    posting.positionTitle = columns.at(titleColumn).toString().trimmed();
    posting.externalSourceId = jobNumber;
    posting.sourceUrl = jobNumber.isEmpty()
        ? QString() : jobPageUrlFor(accountName, careerSectionName, jobNumber);
    posting.companyName = companyNameFromAccountName(accountName);
    posting.postingSource = AtsBoardName::Taleo;

    // The places are a JSON list written inside a string: ["United States-Ohio-Columbus"].
    if (placeColumn >= 0 && placeColumn < columns.count()) {
        const QJsonArray places =
            QJsonDocument::fromJson(columns.at(placeColumn).toString().toUtf8()).array();
        QStringList readablePlaces;
        for (const QJsonValue &place : places) {
            readablePlaces.append(readablePlaceText(place.toString()));
        }
        posting.locationText = readablePlaces.join(QStringLiteral("; "));
    }
    posting.isRemoteRole = posting.locationText.contains(QStringLiteral("remote"),
                                                         Qt::CaseInsensitive);

    // The posting date is whichever other column reads as a date.
    for (int columnIndex = 0; columnIndex < columns.count(); ++columnIndex) {
        if (columnIndex == titleColumn || columnIndex == placeColumn) {
            continue;
        }
        const QDate postedDate = postedDateFromColumnText(columns.at(columnIndex).toString());
        if (postedDate.isValid()) {
            posting.postedTimestamp = QDateTime(postedDate, timeOfDayForADateOnlyPost);
            break;
        }
    }
    posting.discoveredTimestamp = QDateTime::currentDateTime();
    return posting;
}

QString TaleoBoardSource::descriptionFromJobPage(const QString &jobPageHtml)
{
    // How this works (header, point 5). The page holds two lists for the
    // description block: a list of names (_hlid) and, after it, a list of
    // values (the fillList call). The nth name goes with the nth value.
    //
    // Steps:
    //   1. Find the start of the fillList call's value list, the "[" right
    //      after 'descRequisition'. The pattern allows any spaces between
    //      the parts. Not found: return empty.
    //   2. The values run from there to the next "]);". Pull every quoted
    //      string out of that stretch.
    //   3. Look back from the fillList call for the last "descRequisition"
    //      before it. That is where the block starts.
    //   4. From the block start, find "_hlid", then the next "]". Pull every
    //      quoted string out of that stretch: those are the names. If the
    //      names are missing, or come after the fillList call, return empty.
    //   5. Look up the description and the qualifications by name, take the
    //      value at the same position, and turn each into plain text.
    //   6. Join what was found with a blank line between.
    //
    // The values: api.fillList('requisitionDescriptionInterface', 'descRequisition', [...]);
    static const QRegularExpression valueListStartPattern(QStringLiteral(
        "fillList\\(\\s*'requisitionDescriptionInterface'\\s*,\\s*'descRequisition'\\s*,\\s*\\["));
    const QRegularExpressionMatch valueListStart = valueListStartPattern.match(jobPageHtml);
    if (!valueListStart.hasMatch()) {
        return QString();
    }
    const int valuesStart = valueListStart.capturedEnd();
    const int valuesEnd = jobPageHtml.indexOf(QStringLiteral("]);"), valuesStart);
    if (valuesEnd < 0) {
        return QString();
    }
    const QStringList values =
        quotedStringsIn(jobPageHtml.mid(valuesStart, valuesEnd - valuesStart));

    // The names: the _hlid list in the descRequisition block before it.
    const int blockStart = jobPageHtml.lastIndexOf(QStringLiteral("descRequisition"),
                                                   valueListStart.capturedStart() - 1);
    const int namesStart = jobPageHtml.indexOf(QStringLiteral("_hlid"), qMax(0, blockStart));
    const int namesEnd = jobPageHtml.indexOf(QLatin1Char(']'), namesStart);
    if (namesStart < 0 || namesEnd < 0 || namesStart > valueListStart.capturedStart()) {
        return QString();
    }
    const QStringList names = quotedStringsIn(jobPageHtml.mid(namesStart, namesEnd - namesStart));

    QStringList descriptionParts;
    for (const QString &wantedName : { QStringLiteral("reqlistitem.description"),
                                       QStringLiteral("reqlistitem.qualification") }) {
        const int valueIndex = names.indexOf(wantedName);
        if (valueIndex >= 0 && valueIndex < values.count()) {
            const QString partText = plainTextFromTaleoValue(values.at(valueIndex)).trimmed();
            if (!partText.isEmpty()) {
                descriptionParts.append(partText);
            }
        }
    }
    return descriptionParts.join(QStringLiteral("\n\n"));
}

// Steps: look in knownPortalNumbers first. If the site is not there, GET the
// careers section's search page, read the portal number out of its HTML,
// keep it for next time, and then call whenPortalIsKnown. A page that will
// not load, or has no number in it, fails the reply.
void TaleoBoardSource::withPortalNumber(
    const QString &accountName, const QString &careerSectionName, JobScoutReply *scoutReply,
    const std::function<void(const QString &)> &whenPortalIsKnown)
{
    const QString siteKey = QStringLiteral("%1/%2").arg(accountName, careerSectionName).toLower();
    if (knownPortalNumbers.contains(siteKey)) {
        whenPortalIsKnown(knownPortalNumbers.value(siteKey));
        return;
    }

    QNetworkReply *networkReply =
        networkAccessManager.get(pageRequest(searchPageUrlFor(accountName, careerSectionName)));
    networkReply->setParent(scoutReply); // dies with the scout reply
    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [this, networkReply, scoutReply, siteKey, whenPortalIsKnown]() {
        networkReply->deleteLater();
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();
        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                taleoDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }
        const QString portalNumber = portalNumberFromSearchPage(QString::fromUtf8(responseBody));
        if (portalNumber.isEmpty()) {
            qWarning().noquote() << QStringLiteral(
                "TaleoBoardSource::withPortalNumber: no portal number on the search page "
                "for \"%1\"%2")
                    .arg(siteKey, responseDiagnosticTail(httpStatusCode, responseBody));
            scoutReply->markFailed(QStringLiteral(
                "Taleo's careers page didn't have the number Job Crush needs to search it. "
                "The site may be laid out differently from the ones Job Crush knows.%1")
                    .arg(responseDiagnosticTail(httpStatusCode, responseBody)));
            return;
        }
        knownPortalNumbers.insert(siteKey, portalNumber);
        whenPortalIsKnown(portalNumber);
    });
}

// Steps: read the tenant, set up the search state, get the portal number,
// then read the first page. readSearchPage carries on from there.
JobScoutReply *TaleoBoardSource::searchSite(const QString &taleoTenant,
                                            const QString &searchWords,
                                            int pagesAllowed, QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    QString accountName;
    QString careerSectionName;
    if (!readTenant(taleoTenant, accountName, careerSectionName)) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "Job Crush needs a link to one of the company's Taleo jobs before it can look "
            "there. Paste a job link from their careers site."), false);
        return scoutReply;
    }

    auto search = std::make_shared<TaleoSearch>();
    search->scoutReply = scoutReply;
    search->taleoTenant = taleoTenant.trimmed();
    search->accountName = accountName;
    search->searchWords = searchWords.trimmed();
    search->pagesAllowed = pagesAllowed;

    withPortalNumber(accountName, careerSectionName, scoutReply,
                     [this, search](const QString &portalNumber) {
        search->portalNumber = portalNumber;
        readSearchPage(search);
    });
    return scoutReply;
}

// Sends one page of the search as a POST, then reads the answer. Pages are
// counted from 1, so the page asked for is pagesRead + 1. After each page it
// either asks for the next one or finishes the reply.
void TaleoBoardSource::readSearchPage(const std::shared_ptr<TaleoSearch> &search)
{
    QNetworkRequest networkRequest =
        employerBoardRequest(searchServiceUrlFor(search->accountName, search->portalNumber));
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("tz"), timeZoneHeaderValue());

    QNetworkReply *networkReply = networkAccessManager.post(
        networkRequest, searchRequestBody(search->searchWords, search->pagesRead + 1));
    networkReply->setParent(search->scoutReply);

    QObject::connect(networkReply, &QNetworkReply::finished, search->scoutReply,
                     [this, search, networkReply]() {
        networkReply->deleteLater();
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (networkReply->error() != QNetworkReply::NoError) {
            search->scoutReply->markFailed(boardDidNotAnswerMessage(
                taleoDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            search->scoutReply->markFailed(boardSentUnreadableMessage(
                taleoDisplayName, httpStatusCode, responseBody));
            return;
        }

        const QJsonObject pageObject = responseDocument.object();
        if (pageObject.value(QStringLiteral("careerSectionUnAvailable")).toBool()) {
            search->scoutReply->markFailed(QStringLiteral(
                "That Taleo careers page is closed. The company may have moved its jobs "
                "somewhere else. Open their careers page and paste a link from there."),
                false);
            return;
        }

        const QJsonArray requisitions =
            pageObject.value(QStringLiteral("requisitionList")).toArray();
        for (const QJsonValue &requisitionValue : requisitions) {
            const JobPosting posting =
                postingFromRequisition(requisitionValue.toObject(), search->taleoTenant);
            if (!posting.positionTitle.isEmpty() && !posting.externalSourceId.isEmpty()) {
                search->foundPostings.append(posting);
            }
        }
        ++search->pagesRead;

        // More to read when this page had jobs and fewer jobs are kept so far
        // than pagingData.totalCount says there are. Read on only while under
        // the page limit.
        const int totalCount = pageObject.value(QStringLiteral("pagingData")).toObject()
                                   .value(QStringLiteral("totalCount")).toInt();
        const bool moreToRead =
            !requisitions.isEmpty() && search->foundPostings.count() < totalCount;
        if (moreToRead && search->pagesRead < search->pagesAllowed) {
            readSearchPage(search);
            return;
        }

        // Stopped before the end, or only searched: say so, so no job past the
        // stop is ever taken for gone.
        search->scoutReply->setReadOnlyPartOfTheBoard(moreToRead
                                                      || !search->searchWords.isEmpty());
        search->scoutReply->setStoppedAtThePageLimit(moreToRead);
        search->scoutReply->markFinished(search->foundPostings);
    });
}

JobScoutReply *TaleoBoardSource::fetchEveryJobForEmployer(const QString &tenant,
                                                          QObject *replyParent)
{
    return searchSite(tenant, QString(), maxPagesPerSite, replyParent);
}

JobScoutReply *TaleoBoardSource::fetchJobsLikeTitle(const QString &tenant,
                                                    const QString &positionTitle,
                                                    QObject *replyParent)
{
    // Taleo's own search, two pages. The caller picks the right title from it.
    return searchSite(tenant, positionTitle, pagesForATitleSearch, replyParent);
}

JobScoutReply *TaleoBoardSource::fetchOneJob(const AtsBoardIdentity &boardIdentity,
                                             QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    QString accountName;
    QString careerSectionName;
    if (!boardIdentity.namesOneJob()
            || !readTenant(boardIdentity.tenant, accountName, careerSectionName)) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "That link doesn't name a single Taleo job. Open the job on the employer's "
            "careers site and paste the link from there."), false);
        return scoutReply;
    }

    // Searching for the job's number finds exactly that job, or nothing when
    // it is gone. Then, if it was found, its own page is read for the
    // description, which the search does not send.
    const QString jobNumber = boardIdentity.jobId;
    JobScoutReply *searchReply =
        searchSite(boardIdentity.tenant, jobNumber, pagesForOneJobLookup, scoutReply);

    QObject::connect(searchReply, &JobScoutReply::finished, scoutReply,
                     [this, scoutReply, searchReply, jobNumber](
                         const QList<JobPosting> &foundPostings) {
        searchReply->deleteLater();
        JobPosting thisJob;
        for (const JobPosting &foundPosting : foundPostings) {
            if (foundPosting.externalSourceId == jobNumber) {
                thisJob = foundPosting;
                break;
            }
        }
        if (thisJob.externalSourceId.isEmpty()) {
            scoutReply->markFinished({}); // the job is gone; an answer, not an error
            return;
        }

        // The description is on the job's own page. If that page cannot be
        // read, the job is still kept, without its description.
        QNetworkReply *pageReply =
            networkAccessManager.get(pageRequest(QUrl(thisJob.sourceUrl)));
        pageReply->setParent(scoutReply); // dies with the scout reply
        QObject::connect(pageReply, &QNetworkReply::finished, scoutReply,
                         [scoutReply, pageReply, thisJob]() mutable {
            pageReply->deleteLater();
            if (pageReply->error() == QNetworkReply::NoError) {
                thisJob.fullDescriptionText =
                    descriptionFromJobPage(QString::fromUtf8(pageReply->readAll()));
            } else {
                qWarning().noquote() << QStringLiteral(
                    "TaleoBoardSource::fetchOneJob: could not read the job page %1 — %2. "
                    "The job is kept without its description.")
                        .arg(thisJob.sourceUrl, pageReply->errorString());
            }
            scoutReply->markFinished({ thisJob });
        });
    });
    QObject::connect(searchReply, &JobScoutReply::failed, scoutReply,
                     [scoutReply, searchReply](const QString &whyItFailed,
                                               bool sourceHadTrouble) {
        searchReply->deleteLater();
        scoutReply->markFailed(whyItFailed, sourceHadTrouble);
    });
    return scoutReply;
}
