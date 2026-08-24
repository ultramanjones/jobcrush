#include "JobicyJobSource.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QtMath>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

#include "EmployerBoardHttp.h"
#include "JobPostingTextCleanup.h"
#include "JobScoutReply.h"
#include "JobSearchProfile.h"

namespace {

const QString jobicyStorageName = QStringLiteral("jobicy");
const QString jobicyDisplayName = QStringLiteral("Jobicy");
const QString jobicyJobsEndpointUrl = QStringLiteral("https://jobicy.com/api/v2/remote-jobs");

// Where the time of the last check is kept. In settings rather than in memory,
// because closing the app and opening it again is not a new hour.
const QString jobicyLastCheckedKey = QStringLiteral("jobScout/jobicy/lastCheckedAt");

// Jobicy's stated fair use. Job Crush honours it.
constexpr int minutesJobicyAsksUsToWait = 60;

// One sweep's worth. Jobicy's documented ceiling.
constexpr int jobicyMaximumResultsPerSweep = 50;

// The pay line, built from the four separate fields Jobicy sends.
//
// Most job sites hand over pay as free text or not at all. Jobicy gives real
// numbers, so they get written out properly: thousands separated, currency in
// front, and the period spelled out. A number with no period on it is a lie by
// omission — 45 an hour and 45 a year are not the same job.
QString payLineFrom(const QJsonObject &jobObject)
{
    const double lowestPay = jobObject.value(QStringLiteral("salaryMin")).toVariant().toDouble();
    const double highestPay = jobObject.value(QStringLiteral("salaryMax")).toVariant().toDouble();
    if (lowestPay <= 0.0 && highestPay <= 0.0) {
        return QString();
    }

    const QLocale plainLocale(QLocale::English, QLocale::UnitedStates);
    const QString currencyCode =
        jobObject.value(QStringLiteral("salaryCurrency")).toString().trimmed().toUpper();
    const QString currencyMark = currencyCode == QStringLiteral("USD")
        ? QStringLiteral("$")
        : (currencyCode.isEmpty() ? QString() : currencyCode + QLatin1Char(' '));

    // Cents only when there are cents. A salary of $145,000.00 is noise, but
    // rounding $23.50 an hour changes the number the user is deciding on —
    // and hourly rates are exactly where the cents are real.
    auto writtenOut = [&](double amount) {
        const bool hasCents = qAbs(amount - qRound(amount)) > 0.004;
        return currencyMark + plainLocale.toString(amount, 'f', hasCents ? 2 : 0);
    };

    QString payLine;
    if (lowestPay > 0.0 && highestPay > 0.0 && highestPay > lowestPay) {
        payLine = QStringLiteral("%1 – %2").arg(writtenOut(lowestPay), writtenOut(highestPay));
    } else {
        payLine = writtenOut(lowestPay > 0.0 ? lowestPay : highestPay);
    }

    const QString period =
        jobObject.value(QStringLiteral("salaryPeriod")).toString().trimmed().toLower();
    if (period == QStringLiteral("yearly") || period == QStringLiteral("annually")) {
        return payLine + QStringLiteral(" a year");
    }
    if (period == QStringLiteral("monthly")) {
        return payLine + QStringLiteral(" a month");
    }
    if (period == QStringLiteral("weekly")) {
        return payLine + QStringLiteral(" a week");
    }
    if (period == QStringLiteral("daily")) {
        return payLine + QStringLiteral(" a day");
    }
    if (period == QStringLiteral("hourly")) {
        return payLine + QStringLiteral(" an hour");
    }
    // An unknown period is left off rather than guessed at. A wrong period is
    // worse than none: it tells the user something untrue about the money.
    return payLine;
}

// One job out of Jobicy's JSON.
JobPosting postingFromJobicyJob(const QJsonObject &jobObject, const QDateTime &sweepTimestamp)
{
    JobPosting jobPosting;
    jobPosting.discoverySource = jobicyStorageName;

    // The live feed calls this "id". Jobicy's own documentation says "jobId"
    // — it is wrong, and code written from the docs quietly stores every job
    // with a blank id and then loses track of which ones it has already seen.
    jobPosting.externalSourceId =
        QString::number(jobObject.value(QStringLiteral("id")).toVariant().toLongLong());

    jobPosting.positionTitle =
        jobObject.value(QStringLiteral("jobTitle")).toString().trimmed();
    jobPosting.companyName =
        jobObject.value(QStringLiteral("companyName")).toString().trimmed();

    // Jobicy lists remote roles only. jobGeo says where a remote worker is
    // allowed to live, which is a different question and still worth showing.
    jobPosting.isRemoteRole = true;
    jobPosting.locationText = jobObject.value(QStringLiteral("jobGeo")).toString().trimmed();

    jobPosting.salaryText = payLineFrom(jobObject);

    // The canonical link, kept exactly as Jobicy gave it. Their terms ask for
    // that, and it is the right thing anyway: the user should land on the real
    // posting, not on somebody's copy of it.
    jobPosting.sourceUrl = jobObject.value(QStringLiteral("url")).toString().trimmed();

    jobPosting.fullDescriptionText = plainTextFromHtmlFragment(
        jobObject.value(QStringLiteral("jobDescription")).toString());
    if (jobPosting.fullDescriptionText.isEmpty()) {
        jobPosting.fullDescriptionText = plainTextFromHtmlFragment(
            jobObject.value(QStringLiteral("jobExcerpt")).toString());
    }

    jobPosting.postedTimestamp = QDateTime::fromString(
        jobObject.value(QStringLiteral("pubDate")).toString(), Qt::ISODate);
    jobPosting.discoveredTimestamp = sweepTimestamp;

    return jobPosting;
}

// How long until Jobicy may be asked again, or 0 when it may be asked now.
int minutesLeftBeforeJobicyMayBeAskedAgain()
{
    QSettings settings;
    const QDateTime lastChecked =
        settings.value(jobicyLastCheckedKey).toDateTime();
    if (!lastChecked.isValid()) {
        return 0;
    }

    // A clock that has gone backwards — a timezone change, a manual clock
    // set — must not lock the source out for hours. Treat it as due.
    const qint64 minutesSince = lastChecked.secsTo(QDateTime::currentDateTime()) / 60;
    if (minutesSince < 0 || minutesSince >= minutesJobicyAsksUsToWait) {
        return 0;
    }
    return static_cast<int>(minutesJobicyAsksUsToWait - minutesSince);
}

void rememberThatJobicyWasJustChecked()
{
    QSettings settings;
    settings.setValue(jobicyLastCheckedKey, QDateTime::currentDateTime());
}

} // namespace

JobicyJobSource::JobicyJobSource() = default;

JobSourceDescriptor JobicyJobSource::descriptor() const
{
    bool found = false;
    return jobSourceDescriptorFor(jobicyStorageName, found);
}

JobScoutReply *JobicyJobSource::searchForJobs(const JobSearchProfile &searchProfile,
                                              QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    const int minutesToWait = minutesLeftBeforeJobicyMayBeAskedAgain();
    if (minutesToWait > 0) {
        // Not trouble — a rule being kept. The jobs from the last check are
        // still in the Jobicy tab, so there is nothing to go and fix.
        failThisReplyOnceTheCallerIsListening(scoutReply,
            QStringLiteral("checked less than an hour ago, so Job Crush left it alone — "
                           "its jobs are still in the Jobicy tab. Ready again in about "
                           "%1 %2.")
                .arg(minutesToWait)
                .arg(minutesToWait == 1 ? QStringLiteral("minute") : QStringLiteral("minutes")),
            false);
        return scoutReply;
    }

    QUrl requestUrl(jobicyJobsEndpointUrl);
    QUrlQuery requestQuery;
    requestQuery.addQueryItem(QStringLiteral("count"),
                              QString::number(jobicyMaximumResultsPerSweep));

    // Jobicy takes one keyword. The first target title is the user's clearest
    // statement of what they want; everything else is weighed locally by the
    // scorer, which costs nothing and never runs out of requests.
    const QStringList targetJobTitles = searchProfile.targetJobTitles();
    if (!targetJobTitles.isEmpty()) {
        requestQuery.addQueryItem(QStringLiteral("tag"), targetJobTitles.first());
    }
    requestUrl.setQuery(requestQuery);

    QNetworkReply *networkReply =
        networkAccessManager.get(employerBoardRequest(requestUrl));
    networkReply->setParent(scoutReply); // dies with the scout reply

    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (networkReply->error() != QNetworkReply::NoError) {
            // Nothing reached Jobicy, so nothing was spent. Trying again in a
            // minute costs them nothing and may well work.
            scoutReply->markFailed(boardDidNotAnswerMessage(
                jobicyDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }

        // Jobicy answered, so the hour starts now — whatever it said. Starting
        // the clock only on a response Job Crush liked would turn a run of bad
        // answers into a retry loop against a site that asked us to wait.
        rememberThatJobicyWasJustChecked();

        QJsonParseError parseError;
        const QJsonDocument responseDocument =
            QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                jobicyDisplayName, httpStatusCode, responseBody));
            return;
        }

        const QJsonArray jobArray =
            responseDocument.object().value(QStringLiteral("jobs")).toArray();
        if (jobArray.isEmpty()) {
            // An honest empty answer, not a fault.
            scoutReply->markFailed(
                QStringLiteral("answered with no jobs in it. Try a different job title "
                               "in Settings."),
                false);
            return;
        }

        const QDateTime sweepTimestamp = QDateTime::currentDateTime();
        QList<JobPosting> foundJobPostings;

        for (const QJsonValue &jobValue : jobArray) {
            const JobPosting jobPosting =
                postingFromJobicyJob(jobValue.toObject(), sweepTimestamp);

            // A posting with no id cannot be recognized on the next sweep, and
            // one with no link cannot be applied to. Either way it is no use.
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
