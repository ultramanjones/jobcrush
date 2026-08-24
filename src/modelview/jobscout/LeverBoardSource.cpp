#include "LeverBoardSource.h"

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

const QString leverApiBase = QStringLiteral("https://api.lever.co/v0/postings/");
const QString leverDisplayName = QStringLiteral("Lever");

QUrl leverBoardUrl(const QString &tenant)
{
    return QUrl(leverApiBase + QString::fromUtf8(QUrl::toPercentEncoding(tenant))
                + QStringLiteral("?mode=json"));
}

QUrl leverOneJobUrl(const QString &tenant, const QString &jobId)
{
    return QUrl(leverApiBase + QString::fromUtf8(QUrl::toPercentEncoding(tenant))
                + QStringLiteral("/") + QString::fromUtf8(QUrl::toPercentEncoding(jobId))
                + QStringLiteral("?mode=json"));
}

// Lever splits one posting's text across several fields, and the one named
// "descriptionPlain" is only the top of it. The requirements and the
// nice-to-haves live in "lists", which is where the words the scorer needs
// actually are. Reading descriptionPlain alone would score every Lever job
// against its own introduction.
QString wholeDescriptionFrom(const QJsonObject &postingObject)
{
    QStringList descriptionParts;

    const QString openingAndBody =
        postingObject.value(QStringLiteral("descriptionPlain")).toString().trimmed();
    if (!openingAndBody.isEmpty()) {
        descriptionParts.append(openingAndBody);
    }

    const QJsonArray listSections = postingObject.value(QStringLiteral("lists")).toArray();
    for (const QJsonValue &sectionValue : listSections) {
        const QJsonObject sectionObject = sectionValue.toObject();
        const QString sectionHeading =
            sectionObject.value(QStringLiteral("text")).toString().trimmed();
        const QString sectionBody = plainTextFromHtmlFragment(
            sectionObject.value(QStringLiteral("content")).toString());

        if (!sectionHeading.isEmpty()) {
            descriptionParts.append(sectionHeading);
        }
        if (!sectionBody.isEmpty()) {
            descriptionParts.append(sectionBody);
        }
    }

    const QString closingText =
        postingObject.value(QStringLiteral("additionalPlain")).toString().trimmed();
    if (!closingText.isEmpty()) {
        descriptionParts.append(closingText);
    }

    return descriptionParts.join(QStringLiteral("\n\n"));
}

// Where the job is. Lever puts one place in categories.location and every
// place in categories.allLocations. A job open in three cities should say so.
QString locationTextFrom(const QJsonObject &categoriesObject)
{
    const QJsonArray everyLocation =
        categoriesObject.value(QStringLiteral("allLocations")).toArray();

    QStringList placeNames;
    for (const QJsonValue &locationValue : everyLocation) {
        const QString placeName = locationValue.toString().trimmed();
        if (!placeName.isEmpty() && !placeNames.contains(placeName)) {
            placeNames.append(placeName);
        }
    }
    if (!placeNames.isEmpty()) {
        return placeNames.join(QStringLiteral(", "));
    }
    return categoriesObject.value(QStringLiteral("location")).toString().trimmed();
}

JobPosting postingFromLeverJob(const QJsonObject &postingObject, const QString &tenant)
{
    JobPosting posting;

    posting.externalSourceId = postingObject.value(QStringLiteral("id")).toString().trimmed();
    posting.positionTitle = postingObject.value(QStringLiteral("text")).toString().trimmed();
    posting.companyName = companyNameFromTenant(tenant);

    // hostedUrl is the posting itself. applyUrl is the form. The user wants to
    // read the job before filling anything in, so the posting is the link, and
    // the form is only the fallback when a board somehow has no hosted page.
    posting.sourceUrl = postingObject.value(QStringLiteral("hostedUrl")).toString().trimmed();
    if (posting.sourceUrl.isEmpty()) {
        posting.sourceUrl = postingObject.value(QStringLiteral("applyUrl")).toString().trimmed();
    }

    const QJsonObject categoriesObject =
        postingObject.value(QStringLiteral("categories")).toObject();
    posting.locationText = locationTextFrom(categoriesObject);

    posting.fullDescriptionText = wholeDescriptionFrom(postingObject);

    // Lever counts createdAt in milliseconds since 1970, not as a date string.
    const qint64 createdMilliseconds =
        postingObject.value(QStringLiteral("createdAt")).toVariant().toLongLong();
    if (createdMilliseconds > 0) {
        posting.postedTimestamp = QDateTime::fromMSecsSinceEpoch(createdMilliseconds);
    }

    // Lever states this outright, so believe the field first. The location
    // words are only a second look, for boards that leave workplaceType unset.
    const QString workplaceType =
        postingObject.value(QStringLiteral("workplaceType")).toString().toLower();
    const QString locationLowered = posting.locationText.toLower();
    posting.isRemoteRole = workplaceType == QStringLiteral("remote")
                        || locationLowered.contains(QStringLiteral("remote"))
                        || locationLowered.contains(QStringLiteral("anywhere"))
                        || locationLowered.contains(QStringLiteral("distributed"));

    // No salary here on purpose. Lever's public postings do not carry a pay
    // field that shows up reliably across boards, and a field read by a name
    // nobody confirmed is dead code that looks like it works.

    posting.discoverySource = AtsBoardName::Lever;
    posting.discoveredTimestamp = QDateTime::currentDateTime();
    return posting;
}

} // namespace

LeverBoardSource::LeverBoardSource() = default;

QString LeverBoardSource::boardName() const
{
    return AtsBoardName::Lever;
}

JobScoutReply *LeverBoardSource::fetchEveryJobForEmployer(const QString &tenant,
                                                          QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    const QString trimmedTenant = tenant.trimmed();
    if (trimmedTenant.isEmpty()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "Job Crush needs the company's Lever name before it can look. Paste a link "
            "to one of their jobs and it will work the name out."));
        return scoutReply;
    }

    QNetworkReply *networkReply =
        networkAccessManager.get(employerBoardRequest(leverBoardUrl(trimmedTenant)));
    networkReply->setParent(scoutReply); // dies with the scout reply

    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply, trimmedTenant]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        if (httpStatusCode == 404) {
            scoutReply->markFailed(noSuchBoardMessage(leverDisplayName, trimmedTenant), false);
            return;
        }
        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                leverDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument =
            QJsonDocument::fromJson(responseBody, &parseError);

        // A whole board comes back as an array. Anything else is not a board.
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isArray()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                leverDisplayName, httpStatusCode, responseBody));
            return;
        }

        QList<JobPosting> foundPostings;
        const QJsonArray postingArray = responseDocument.array();
        for (const QJsonValue &postingValue : postingArray) {
            const JobPosting posting =
                postingFromLeverJob(postingValue.toObject(), trimmedTenant);

            // No title or no link means nothing to show and nothing to open.
            // No id means Job Crush cannot recognize it on the next sweep, so
            // it would be stored again every single time.
            if (posting.positionTitle.isEmpty() || posting.sourceUrl.isEmpty()
                    || posting.externalSourceId.isEmpty()) {
                continue;
            }
            foundPostings.append(posting);
        }
        scoutReply->markFinished(foundPostings);
    });

    return scoutReply;
}

JobScoutReply *LeverBoardSource::fetchOneJob(const AtsBoardIdentity &boardIdentity,
                                             QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    if (!boardIdentity.namesOneJob()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "That link doesn't name a single Lever job. Open the job on the employer's "
            "site and paste the link from there."));
        return scoutReply;
    }

    QNetworkReply *networkReply = networkAccessManager.get(employerBoardRequest(
        leverOneJobUrl(boardIdentity.tenant, boardIdentity.jobId)));
    networkReply->setParent(scoutReply);

    const QString tenant = boardIdentity.tenant;
    QObject::connect(networkReply, &QNetworkReply::finished, scoutReply,
                     [scoutReply, networkReply, tenant]() {
        const int httpStatusCode =
            networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseBody = networkReply->readAll();

        // A job that is gone is not an error. It is the answer, and it is one
        // the user needs: a closed job should stop looking open on the board.
        if (httpStatusCode == 404) {
            scoutReply->markFinished({});
            return;
        }
        if (networkReply->error() != QNetworkReply::NoError) {
            scoutReply->markFailed(boardDidNotAnswerMessage(
                leverDisplayName, networkReply->errorString(), httpStatusCode, responseBody));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument responseDocument =
            QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !responseDocument.isObject()) {
            scoutReply->markFailed(boardSentUnreadableMessage(
                leverDisplayName, httpStatusCode, responseBody));
            return;
        }

        const JobPosting posting = postingFromLeverJob(responseDocument.object(), tenant);
        if (posting.positionTitle.isEmpty()) {
            scoutReply->markFinished({});
            return;
        }
        scoutReply->markFinished({ posting });
    });

    return scoutReply;
}
