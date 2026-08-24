#include "UsaJobsJobSource.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QtMath>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"
#include "JobSearchProfile.h"
#include "JobSourceRoster.h"

namespace {

const QString usaJobsStorageName = QStringLiteral("usajobs");
const QString usaJobsDisplayName = QStringLiteral("USAJOBS");
const QString usaJobsSearchEndpointUrl = QStringLiteral("https://data.usajobs.gov/api/Search");

// USAJOBS allows 500 a page. This is one sweep's worth: enough to be useful,
// small enough to come back quickly on a home connection.
constexpr int usaJobsMaximumResultsPerSweep = 100;

// How pay is counted. USAJOBS sends the number and the interval separately,
// and printing the number without the interval is wrong by a factor of two
// thousand — $45 an hour read as $45 a year.
QString payIntervalWordsFor(const QString &rateIntervalCode)
{
    const QString code = rateIntervalCode.trimmed().toUpper();
    if (code == QStringLiteral("PA")) return QStringLiteral(" a year");
    if (code == QStringLiteral("PH")) return QStringLiteral(" an hour");
    if (code == QStringLiteral("PD")) return QStringLiteral(" a day");
    if (code == QStringLiteral("PW")) return QStringLiteral(" a week");
    if (code == QStringLiteral("BW")) return QStringLiteral(" every two weeks");
    if (code == QStringLiteral("PM")) return QStringLiteral(" a month");
    if (code == QStringLiteral("SY")) return QStringLiteral(" a school year");
    // A code nobody here recognizes is left off rather than guessed at. A
    // wrong interval tells the user something untrue about the money.
    return QString();
}

QString payLineFrom(const QJsonArray &remunerationArray)
{
    if (remunerationArray.isEmpty()) {
        return QString();
    }
    const QJsonObject payObject = remunerationArray.first().toObject();

    // WC is "without compensation" — a volunteer posting. It comes through
    // with zeroes in the money fields, so without this it would look exactly
    // like a job that simply did not publish its pay. That is worth saying
    // out loud: somebody who applies for a paid job and finds out later that
    // it is unpaid has lost the afternoon this app exists to save them.
    const QString rateIntervalCode =
        payObject.value(QStringLiteral("RateIntervalCode")).toString().trimmed().toUpper();
    if (rateIntervalCode == QStringLiteral("WC")) {
        return QStringLiteral("Unpaid");
    }

    const double lowestPay =
        payObject.value(QStringLiteral("MinimumRange")).toVariant().toDouble();
    const double highestPay =
        payObject.value(QStringLiteral("MaximumRange")).toVariant().toDouble();
    if (lowestPay <= 0.0 && highestPay <= 0.0) {
        return QString();
    }

    const QLocale plainLocale(QLocale::English, QLocale::UnitedStates);

    // Cents only when there are cents. A salary of $72,553.00 is noise, but
    // rounding $23.50 an hour to $24 changes the number the user is deciding
    // on — and hourly rates are exactly where the cents are real.
    auto writtenOut = [&plainLocale](double amount) {
        const bool hasCents = qAbs(amount - qRound(amount)) > 0.004;
        return QStringLiteral("$") + plainLocale.toString(amount, 'f', hasCents ? 2 : 0);
    };

    QString payLine;
    if (lowestPay > 0.0 && highestPay > lowestPay) {
        payLine = QStringLiteral("%1 – %2").arg(writtenOut(lowestPay), writtenOut(highestPay));
    } else {
        payLine = writtenOut(lowestPay > 0.0 ? lowestPay : highestPay);
    }

    return payLine + payIntervalWordsFor(rateIntervalCode);
}

// Who is allowed to apply, first and in plain sight.
//
// A large share of federal postings are open only to people who already work
// for the government, or to veterans, or to one agency's own staff. Someone
// who reads the whole announcement and only then finds out they were never
// eligible has lost an afternoon, and this app exists to stop exactly that
// kind of waste. So this goes at the TOP of the description, which puts it on
// the Discoveries card as well.
QString whoMayApplyLineFrom(const QJsonObject &detailsObject)
{
    const QJsonValue whoMayApplyValue = detailsObject.value(QStringLiteral("WhoMayApply"));

    // USAJOBS sends this as an object with a Name on it. Older announcements
    // in the same feed carry a plain string instead, so both are read.
    QString whoMayApply = whoMayApplyValue.isObject()
        ? whoMayApplyValue.toObject().value(QStringLiteral("Name")).toString()
        : whoMayApplyValue.toString();
    whoMayApply = whoMayApply.trimmed();

    if (whoMayApply.isEmpty()) {
        return QString();
    }
    return QStringLiteral("Who may apply: %1").arg(whoMayApply);
}

QString wholeDescriptionFrom(const QJsonObject &descriptorObject)
{
    const QJsonObject detailsObject = descriptorObject.value(QStringLiteral("UserArea"))
        .toObject().value(QStringLiteral("Details")).toObject();

    QStringList descriptionParts;

    const QString whoMayApply = whoMayApplyLineFrom(detailsObject);
    if (!whoMayApply.isEmpty()) {
        descriptionParts.append(whoMayApply);
    }

    const QString jobSummary = plainTextFromHtmlFragment(
        detailsObject.value(QStringLiteral("JobSummary")).toString());
    if (!jobSummary.isEmpty()) {
        descriptionParts.append(jobSummary);
    }

    // MajorDuties is a list of paragraphs, and it is the part that actually
    // describes the work.
    const QJsonArray majorDuties = detailsObject.value(QStringLiteral("MajorDuties")).toArray();
    if (!majorDuties.isEmpty()) {
        QStringList dutyLines;
        for (const QJsonValue &dutyValue : majorDuties) {
            const QString duty = plainTextFromHtmlFragment(dutyValue.toString());
            if (!duty.isEmpty()) {
                dutyLines.append(duty);
            }
        }
        if (!dutyLines.isEmpty()) {
            descriptionParts.append(QStringLiteral("What you would do"));
            descriptionParts.append(dutyLines.join(QStringLiteral("\n")));
        }
    }

    // The qualifications decide whether it is worth applying at all, so the
    // scorer needs them as much as the user does.
    const QString qualifications = plainTextFromHtmlFragment(
        descriptorObject.value(QStringLiteral("QualificationSummary")).toString());
    if (!qualifications.isEmpty()) {
        descriptionParts.append(QStringLiteral("What they are looking for"));
        descriptionParts.append(qualifications);
    }

    const QString requirements = plainTextFromHtmlFragment(
        detailsObject.value(QStringLiteral("Requirements")).toString());
    if (!requirements.isEmpty()) {
        descriptionParts.append(requirements);
    }

    return descriptionParts.join(QStringLiteral("\n\n"));
}

JobPosting postingFromUsaJobsItem(const QJsonObject &searchResultItem,
                                  const QDateTime &sweepTimestamp)
{
    const QJsonObject descriptorObject =
        searchResultItem.value(QStringLiteral("MatchedObjectDescriptor")).toObject();

    JobPosting jobPosting;
    jobPosting.discoverySource = usaJobsStorageName;

    // MatchedObjectId is the announcement's control number. PositionID sits
    // right next to it and looks like an id, but it is the agency's own
    // reference and two announcements can share one.
    jobPosting.externalSourceId =
        searchResultItem.value(QStringLiteral("MatchedObjectId")).toString().trimmed();

    jobPosting.positionTitle =
        descriptorObject.value(QStringLiteral("PositionTitle")).toString().trimmed();

    // The agency is the employer. DepartmentName is the department above it —
    // "Department of Veterans Affairs" over "Veterans Health Administration" —
    // and the agency is the one a person would name if you asked who they
    // work for.
    jobPosting.companyName =
        descriptorObject.value(QStringLiteral("OrganizationName")).toString().trimmed();
    if (jobPosting.companyName.isEmpty()) {
        jobPosting.companyName =
            descriptorObject.value(QStringLiteral("DepartmentName")).toString().trimmed();
    }

    jobPosting.locationText =
        descriptorObject.value(QStringLiteral("PositionLocationDisplay")).toString().trimmed();

    jobPosting.sourceUrl =
        descriptorObject.value(QStringLiteral("PositionURI")).toString().trimmed();
    if (jobPosting.sourceUrl.isEmpty()) {
        jobPosting.sourceUrl =
            descriptorObject.value(QStringLiteral("ApplyURI")).toArray()
                .first().toString().trimmed();
    }

    jobPosting.salaryText = payLineFrom(
        descriptorObject.value(QStringLiteral("PositionRemuneration")).toArray());

    jobPosting.fullDescriptionText = wholeDescriptionFrom(descriptorObject);

    jobPosting.postedTimestamp = QDateTime::fromString(
        descriptorObject.value(QStringLiteral("PublicationStartDate")).toString(),
        Qt::ISODate);

    // USAJOBS has no remote field in its answers. RemoteIndicator is a filter
    // you can ask WITH and never get back, so this is read from the location
    // words. It is right often enough to be worth having and is never used to
    // hide a job — the location filter only ever holds jobs back, and a job
    // wrongly unmarked simply gets judged on its city instead.
    const QString locationLowered = jobPosting.locationText.toLower();
    jobPosting.isRemoteRole = locationLowered.contains(QStringLiteral("remote"))
                           || locationLowered.contains(QStringLiteral("telework"))
                           || locationLowered.contains(QStringLiteral("anywhere"));

    jobPosting.discoveredTimestamp = sweepTimestamp;
    return jobPosting;
}

} // namespace

UsaJobsJobSource::UsaJobsJobSource(const JobSourceRoster &sourceRoster)
    : sourceRoster(sourceRoster)
{
}

JobSourceDescriptor UsaJobsJobSource::descriptor() const
{
    bool found = false;
    return jobSourceDescriptorFor(usaJobsStorageName, found);
}

JobScoutReply *UsaJobsJobSource::searchForJobs(const JobSearchProfile &searchProfile,
                                               QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    const QString accessKey = sourceRoster.accessKeyFor(usaJobsStorageName);
    const QString registeredEmail = sourceRoster.registeredEmailFor(usaJobsStorageName);

    if (accessKey.isEmpty() || registeredEmail.isEmpty()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "needs its free key and the email you registered it with. Both go in "
            "Settings, under JobScout."));
        return scoutReply;
    }

    QUrl requestUrl(usaJobsSearchEndpointUrl);
    QUrlQuery requestQuery;
    requestQuery.addQueryItem(QStringLiteral("ResultsPerPage"),
                              QString::number(usaJobsMaximumResultsPerSweep));
    requestQuery.addQueryItem(QStringLiteral("Page"), QStringLiteral("1"));

    // Full, or the answer arrives without the summary, the duties, or the
    // line saying who may apply — which is most of what makes a federal
    // posting worth reading.
    requestQuery.addQueryItem(QStringLiteral("Fields"), QStringLiteral("Full"));

    // Newest first. Without this the first page is whatever USAJOBS happens
    // to order by, and a job search wants the new ones.
    requestQuery.addQueryItem(QStringLiteral("SortField"), QStringLiteral("opendate"));
    requestQuery.addQueryItem(QStringLiteral("SortDirection"), QStringLiteral("desc"));

    const QStringList targetJobTitles = searchProfile.targetJobTitles();
    if (!targetJobTitles.isEmpty()) {
        requestQuery.addQueryItem(QStringLiteral("Keyword"), targetJobTitles.first());
    }

    // USAJOBS takes one place, spelled its own way ("Denver, Colorado"). The
    // first place the user named is the one asked for; everything else is
    // filtered locally afterwards, which costs nothing.
    const QStringList preferredLocations = searchProfile.preferredWorkLocations();
    for (const QString &preferredLocation : preferredLocations) {
        const QString trimmedLocation = preferredLocation.trimmed();
        // "Remote" is not a place USAJOBS knows. Asking for it by name
        // returns nothing at all, which looks exactly like a broken key.
        if (trimmedLocation.isEmpty()
                || trimmedLocation.compare(QStringLiteral("remote"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        requestQuery.addQueryItem(QStringLiteral("LocationName"), trimmedLocation);
        break;
    }

    requestUrl.setQuery(requestQuery);

    QNetworkRequest networkRequest(requestUrl);
    networkRequest.setRawHeader(QByteArrayLiteral("Accept"),
                                QByteArrayLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("Host"),
                                QByteArrayLiteral("data.usajobs.gov"));

    // The email address, not a browser string and not this app's name.
    // USAJOBS documents this and refuses requests that get it wrong.
    networkRequest.setRawHeader(QByteArrayLiteral("User-Agent"),
                                registeredEmail.toUtf8());
    networkRequest.setRawHeader(QByteArrayLiteral("Authorization-Key"),
                                accessKey.toUtf8());
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *networkReply = networkAccessManager.get(networkRequest);
    networkReply->setParent(scoutReply); // dies with the scout reply

    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        // 401 and 403 both mean the same thing to the person reading it, and
        // the fix is the same one. Say it once, plainly.
        if (httpStatusCode == 401 || httpStatusCode == 403) {
            scoutReply->markFailed(QStringLiteral(
                "turned the request down. Check the key in Settings, and check that the "
                "email there is the exact one you registered the key with — USAJOBS "
                "checks both."));
            return;
        }

        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                usaJobsDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument =
            QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                usaJobsDisplayName, httpStatusCode, responseBody));
            return;
        }

        const QJsonArray searchResultItems =
            responseDocument.object().value(QStringLiteral("SearchResult")).toObject()
                .value(QStringLiteral("SearchResultItems")).toArray();

        if (searchResultItems.isEmpty()) {
            // An honest empty answer, not a fault. Painting it red would send
            // the user checking a key that is working perfectly well.
            scoutReply->markFailed(QStringLiteral(
                "found nothing this time. Try a broader job title in Settings, or a "
                "bigger city — USAJOBS matches place names exactly."),
                false);
            return;
        }

        const QDateTime sweepTimestamp = QDateTime::currentDateTime();
        QList<JobPosting> foundJobPostings;

        for (const QJsonValue &itemValue : searchResultItems) {
            const JobPosting jobPosting =
                postingFromUsaJobsItem(itemValue.toObject(), sweepTimestamp);

            // No id cannot be recognized next sweep; no link cannot be applied
            // to. Either way it is no use.
            if (jobPosting.externalSourceId.isEmpty() || jobPosting.sourceUrl.isEmpty()) {
                continue;
            }
            foundJobPostings.append(jobPosting);
        }

        scoutReply->markFinished(foundJobPostings);
    });

    return scoutReply;
}
