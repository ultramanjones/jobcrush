#include "PostedJobPageReader.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

#include "AtsBoardIdentity.h"
#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"
#include "UrlCanonicalizer.h"

namespace {

// A job page is text. Anything past this is not a job page, it is a mistake,
// and reading all of it into memory helps nobody.
constexpr qint64 mostBytesWorthReading = 4 * 1024 * 1024;

// How long one page may take to arrive, in milliseconds, before the request
// is dropped. Twenty seconds is far longer than a working site needs.
constexpr int pageTransferTimeoutMilliseconds = 20000;

// The length of a plain date written the ISO way: "2026-10-25".
constexpr int isoDateLength = 10;

// A name that may arrive as a string or as an object with a "name" field.
// JSON-LD allows both and sites use both.
QString nameOf(const QJsonValue &value)
{
    if (value.isString()) {
        return value.toString().trimmed();
    }
    if (value.isObject()) {
        return value.toObject().value(QStringLiteral("name")).toString().trimmed();
    }
    return QString();
}

// True when the block's @type is "JobPosting". The type can be one string or
// a list of strings.
bool isAJobPosting(const QJsonObject &block)
{
    const QJsonValue typeValue = block.value(QStringLiteral("@type"));
    if (typeValue.isString()) {
        return typeValue.toString().compare(QStringLiteral("JobPosting"),
                                            Qt::CaseInsensitive) == 0;
    }
    if (typeValue.isArray()) {
        for (const QJsonValue &oneType : typeValue.toArray()) {
            if (oneType.toString().compare(QStringLiteral("JobPosting"),
                                           Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    return false;
}

// Finds the JobPosting block inside one parsed JSON-LD value. The value may
// be the block itself, a list of blocks, or an object holding a list under
// "@graph". Empty when there is none.
QJsonObject jobPostingBlockIn(const QJsonValue &value)
{
    if (value.isArray()) {
        for (const QJsonValue &item : value.toArray()) {
            const QJsonObject found = jobPostingBlockIn(item);
            if (!found.isEmpty()) {
                return found;
            }
        }
        return QJsonObject();
    }
    if (!value.isObject()) {
        return QJsonObject();
    }
    const QJsonObject object = value.toObject();
    if (isAJobPosting(object)) {
        return object;
    }
    if (object.contains(QStringLiteral("@graph"))) {
        return jobPostingBlockIn(object.value(QStringLiteral("@graph")));
    }
    return QJsonObject();
}

// One place, written the way a person would: "Pittsburgh, PA, US".
QString placeFrom(const QJsonValue &jobLocationValue)
{
    if (jobLocationValue.isString()) {
        return jobLocationValue.toString().trimmed();
    }
    if (!jobLocationValue.isObject()) {
        return QString();
    }
    const QJsonObject location = jobLocationValue.toObject();
    const QJsonValue addressValue = location.value(QStringLiteral("address"));
    if (addressValue.isString()) {
        return addressValue.toString().trimmed();
    }
    const QJsonObject address = addressValue.toObject();

    QStringList parts;
    for (const QString &fieldName : { QStringLiteral("addressLocality"),
                                      QStringLiteral("addressRegion"),
                                      QStringLiteral("addressCountry") }) {
        const QString part = nameOf(address.value(fieldName));
        if (!part.isEmpty()) {
            parts.append(part);
        }
    }
    if (parts.isEmpty()) {
        return nameOf(location);
    }
    return parts.join(QStringLiteral(", "));
}

// Every place the block names, joined. jobLocation is one object or a list.
QString placesFrom(const QJsonValue &jobLocationValue)
{
    if (!jobLocationValue.isArray()) {
        return placeFrom(jobLocationValue);
    }
    QStringList places;
    for (const QJsonValue &oneLocation : jobLocationValue.toArray()) {
        const QString place = placeFrom(oneLocation);
        if (!place.isEmpty() && !places.contains(place)) {
            places.append(place);
        }
    }
    return places.join(QStringLiteral(" · "));
}

// Pay, when the block has it, as the posting would say it: "$120,000 -
// $150,000 per year". Empty when there is nothing worth showing.
QString payTextFrom(const QJsonValue &baseSalaryValue)
{
    if (!baseSalaryValue.isObject()) {
        return QString();
    }
    const QJsonObject baseSalary = baseSalaryValue.toObject();
    const QString currency = baseSalary.value(QStringLiteral("currency")).toString();
    const QJsonValue amountValue = baseSalary.value(QStringLiteral("value"));

    auto moneyText = [&currency](double amount) {
        const QString number = QString::number(amount, 'f', 0);
        return currency.isEmpty() ? number : currency + QLatin1Char(' ') + number;
    };

    QString range;
    QString period;
    if (amountValue.isObject()) {
        const QJsonObject amount = amountValue.toObject();
        const double low = amount.value(QStringLiteral("minValue")).toDouble();
        const double high = amount.value(QStringLiteral("maxValue")).toDouble();
        const double single = amount.value(QStringLiteral("value")).toDouble();
        if (low > 0 && high > 0) {
            range = moneyText(low) + QStringLiteral(" - ") + moneyText(high);
        } else if (single > 0) {
            range = moneyText(single);
        } else if (low > 0) {
            range = moneyText(low);
        }
        period = amount.value(QStringLiteral("unitText")).toString().toLower();
    } else if (amountValue.isDouble() && amountValue.toDouble() > 0) {
        range = moneyText(amountValue.toDouble());
    }
    if (range.isEmpty()) {
        return QString();
    }
    return period.isEmpty() ? range : range + QStringLiteral(" per ") + period;
}

// The date the block gives, read from the few shapes sites use.
//
// First it is read as a full ISO date and time: "2026-10-25T09:30:00Z". If
// that fails, only the first ten characters are read, as a plain date at
// midnight. That covers "2026-10-25" and dates with an odd time part after
// them. An empty or unreadable date gives an invalid QDateTime.
QDateTime dateFrom(const QString &dateText)
{
    const QString trimmedDateText = dateText.trimmed();
    if (trimmedDateText.isEmpty()) {
        return QDateTime();
    }
    QDateTime parsedDate = QDateTime::fromString(trimmedDateText, Qt::ISODate);
    if (!parsedDate.isValid()) {
        parsedDate = QDateTime(
            QDate::fromString(trimmedDateText.left(isoDateLength), Qt::ISODate), QTime(0, 0));
    }
    return parsedDate.isValid() ? parsedDate : QDateTime();
}

// The content of one <meta> tag, by its name or property. Empty when the
// page has none.
QString metaContentIn(const QString &pageHtml, const QString &nameOrProperty)
{
    // Both orders occur: content before name, and name before content, so
    // there are two patterns. Both ignore case.
    //
    // nameFirst is: "<meta", then any characters that are not ">", then
    // "name" or "property", "=", a quote, the wanted name, a quote, then
    // more characters that are not ">", then "content", "=", a quote, the
    // content (captured), and a quote. Spaces are allowed around each "=".
    //
    // contentFirst is the same with the two attributes the other way round.
    //
    // The wanted name is escaped first, so a name like "og:title" is
    // matched as plain text and not read as pattern symbols.
    const QString escapedName = QRegularExpression::escape(nameOrProperty);
    const QRegularExpression nameFirst(
        QStringLiteral("<meta[^>]*(?:name|property)\\s*=\\s*[\"']%1[\"'][^>]*content\\s*=\\s*[\"']([^\"']*)[\"']")
            .arg(escapedName),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression contentFirst(
        QStringLiteral("<meta[^>]*content\\s*=\\s*[\"']([^\"']*)[\"'][^>]*(?:name|property)\\s*=\\s*[\"']%1[\"']")
            .arg(escapedName),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = nameFirst.match(pageHtml);
    if (!match.hasMatch()) {
        match = contentFirst.match(pageHtml);
    }
    if (!match.hasMatch()) {
        return QString();
    }
    QString content = match.captured(1);
    decodeOneLayerOfCharacterEntities(content);
    return content.simplified();
}

// The page's <title>, with the site's name cut off the end when it is
// written the usual way: "Senior Engineer - Acme" or "Senior Engineer | Acme".
//
// The steps, in order:
//  1. Find the text between "<title ...>" and "</title>". The "?" stops at
//     the first "</title>", and the match may span line breaks.
//  2. Decode one layer of character entities and squeeze the white space.
//  3. Split the title on a separator with a space on each side: "|", a
//     dash, a bullet, a middle dot or ":". The spaces matter, so a word
//     with a dash in it, like "Full-Stack", is not split.
//  4. With two or more pieces, the first is the job title and the last is
//     the site's name, which goes into siteNameOut. With one piece, the
//     whole title comes back and siteNameOut is not touched.
QString pageTitleIn(const QString &pageHtml, QString &siteNameOut)
{
    static const QRegularExpression titleTagPattern(
        QStringLiteral("<title[^>]*>(.*?)</title>"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = titleTagPattern.match(pageHtml);
    if (!match.hasMatch()) {
        return QString();
    }
    QString title = match.captured(1);
    decodeOneLayerOfCharacterEntities(title);
    title = title.simplified();

    static const QRegularExpression siteNameSeparatorPattern(
        QStringLiteral("\\s+[\\|\\-–—•·:]\\s+"));
    const QStringList pieces = title.split(siteNameSeparatorPattern, Qt::SkipEmptyParts);
    if (pieces.count() >= 2) {
        siteNameOut = pieces.last().trimmed();
        return pieces.first().trimmed();
    }
    return title;
}

} // namespace

PostedJobPageReader::PostedJobPageReader(QObject *parent)
    : QObject(parent)
{
}

bool PostedJobPageReader::lastReadUsedThePageTitleOnly() const
{
    return storedLastReadUsedThePageTitleOnly;
}

bool PostedJobPageReader::readJobFromHtml(const QString &pageHtml, const QString &pageUrl,
                                          JobPosting &jobPosting,
                                          bool &usedThePageTitleOnly) const
{
    usedThePageTitleOnly = false;
    jobPosting = JobPosting();

    const UrlCanonicalizer canonicalizer;
    jobPosting.sourceUrl = pageUrl.trimmed();
    jobPosting.externalSourceId = canonicalizer.canonicalFormOf(pageUrl);
    jobPosting.postingSource = AtsBoardName::EmployerWebsite;

    // --- Way 1: the structured block --------------------------------------
    //
    // The pattern finds each "<script ... type="application/ld+json" ...>"
    // tag, with either kind of quote, and captures everything up to the
    // first "</script>" after it. It ignores case and may span line breaks.
    // A page can hold several of these blocks, so every one is tried in
    // order until one holds a JobPosting with a title.
    static const QRegularExpression jsonLdScriptPattern(
        QStringLiteral("<script[^>]*type\\s*=\\s*[\"']application/ld\\+json[\"'][^>]*>(.*?)</script>"),
        QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatchIterator scriptMatches = jsonLdScriptPattern.globalMatch(pageHtml);
    while (scriptMatches.hasNext()) {
        const QString blockText = scriptMatches.next().captured(1).trimmed();
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(blockText.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            // A block the site wrote badly. The next one may be fine.
            qWarning().noquote() << QStringLiteral(
                "PostedJobPageReader::readJobFromHtml: skipped a JSON-LD block on %1 "
                "that is not valid JSON — %2").arg(pageUrl, parseError.errorString());
            continue;
        }
        const QJsonObject block = jobPostingBlockIn(
            document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object()));
        if (block.isEmpty()) {
            continue;
        }

        jobPosting.positionTitle = block.value(QStringLiteral("title")).toString().simplified();
        if (jobPosting.positionTitle.isEmpty()) {
            continue;   // a JobPosting with no title is no use
        }
        jobPosting.companyName = nameOf(block.value(QStringLiteral("hiringOrganization")));
        jobPosting.locationText = placesFrom(block.value(QStringLiteral("jobLocation")));
        jobPosting.isRemoteRole =
            block.value(QStringLiteral("jobLocationType")).toString()
                .compare(QStringLiteral("TELECOMMUTE"), Qt::CaseInsensitive) == 0;
        if (jobPosting.isRemoteRole && jobPosting.locationText.isEmpty()) {
            jobPosting.locationText = QStringLiteral("Remote");
        }
        jobPosting.fullDescriptionText =
            plainTextFromHtmlFragment(block.value(QStringLiteral("description")).toString());
        jobPosting.salaryText = payTextFrom(block.value(QStringLiteral("baseSalary")));
        jobPosting.postedTimestamp =
            dateFrom(block.value(QStringLiteral("datePosted")).toString());

        // The block's own link is the posting's real address when it has one.
        // The pasted link may carry a tracking tail or point at a redirect.
        const QString blockUrl = block.value(QStringLiteral("url")).toString().trimmed();
        if (blockUrl.startsWith(QStringLiteral("http"))) {
            jobPosting.sourceUrl = blockUrl;
            jobPosting.externalSourceId = canonicalizer.canonicalFormOf(blockUrl);
        }
        return true;
    }

    // --- Way 2: the page title ----------------------------------------------
    QString siteName;
    // The og:title meta tag is usually the cleaner title, so it is tried
    // first. The <title> tag is read either way, because it is also where
    // the site's name comes from.
    QString title = metaContentIn(pageHtml, QStringLiteral("og:title"));
    if (title.isEmpty()) {
        title = pageTitleIn(pageHtml, siteName);
    } else {
        pageTitleIn(pageHtml, siteName);
    }
    if (title.isEmpty()) {
        return false;
    }

    QString company = metaContentIn(pageHtml, QStringLiteral("og:site_name"));
    if (company.isEmpty()) {
        company = siteName;
    }

    // A title that is only the site's name is a home page, not a job.
    if (!company.isEmpty() && title.compare(company, Qt::CaseInsensitive) == 0) {
        return false;
    }

    usedThePageTitleOnly = true;
    jobPosting.positionTitle = title;
    jobPosting.companyName = company;
    jobPosting.fullDescriptionText = metaContentIn(pageHtml, QStringLiteral("description"));
    if (jobPosting.fullDescriptionText.isEmpty()) {
        jobPosting.fullDescriptionText =
            metaContentIn(pageHtml, QStringLiteral("og:description"));
    }
    return true;
}

JobScoutReply *PostedJobPageReader::readJobFromPage(const QString &pageUrl,
                                                    QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);
    storedLastReadUsedThePageTitleOnly = false;

    const QUrl url(pageUrl.trimmed());
    if (!url.isValid() || url.host().isEmpty()
            || (url.scheme() != QStringLiteral("http")
                && url.scheme() != QStringLiteral("https"))) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "That doesn't look like a web link. Copy the job's address out of the "
            "browser's address bar and paste the whole thing."), false);
        return scoutReply;
    }

    // A web page, not a JSON feed, so the Accept header says so. The
    // User-Agent is the same honest one the board readers send.
    QNetworkRequest networkRequest = employerBoardRequest(url);
    networkRequest.setRawHeader(QByteArrayLiteral("Accept"),
                                QByteArrayLiteral("text/html,application/xhtml+xml"));
    networkRequest.setRawHeader(QByteArrayLiteral("Accept-Language"),
                                QByteArrayLiteral("en-US,en"));
    networkRequest.setTransferTimeout(pageTransferTimeoutMilliseconds);

    QNetworkReply *networkReply = networkAccessManager.get(networkRequest);
    networkReply->setParent(scoutReply);

    // Stop reading a page that is clearly not a job page.
    QObject::connect(networkReply, &QNetworkReply::downloadProgress, scoutReply,
                     [networkReply](qint64 bytesReceived, qint64) {
        if (bytesReceived > mostBytesWorthReading) {
            networkReply->abort();
        }
    });

    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [this, networkReply, scoutReply, pageUrl]() {
        const int httpStatusCode = networkReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (networkReply->error() != QNetworkReply::NoError) {
            // 401, 403 and 429 mean the site refused: not allowed, forbidden,
            // or too many requests. 404 and 410 mean the page is not there.
            // Anything else is reported with the network's own words.
            if (httpStatusCode == 403 || httpStatusCode == 401 || httpStatusCode == 429) {
                scoutReply->markFailed(QStringLiteral(
                    "The site turned Job Crush away (HTTP %1) — some sites refuse "
                    "anything that isn't a browser.").arg(httpStatusCode), true);
                return;
            }
            if (httpStatusCode == 404 || httpStatusCode == 410) {
                scoutReply->markFailed(QStringLiteral(
                    "That page is gone (HTTP %1) — the job may have been taken down. "
                    "Check the link in a browser.").arg(httpStatusCode), false);
                return;
            }
            scoutReply->markFailed(
                QStringLiteral("The page didn't load — %1.%2")
                    .arg(networkReply->errorString(),
                         responseDiagnosticTail(httpStatusCode, responseBody)),
                true);
            return;
        }

        const QString pageHtml = QString::fromUtf8(responseBody);
        JobPosting jobPosting;
        bool usedThePageTitleOnly = false;
        if (!readJobFromHtml(pageHtml, pageUrl, jobPosting, usedThePageTitleOnly)) {
            scoutReply->markFinished({});
            return;
        }
        storedLastReadUsedThePageTitleOnly = usedThePageTitleOnly;
        scoutReply->markFinished({ jobPosting });
    });

    return scoutReply;
}
