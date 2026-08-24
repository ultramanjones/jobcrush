#pragma once

#include <QByteArray>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "JobScoutReply.h"

// EmployerBoardHttp
//
// The small pieces every employer board reader needs, in one place.
//
// Greenhouse, Lever and Ashby all work the same way: one plain GET to a public
// URL, JSON comes back, parse it. Only the URL and the field names differ. The
// request headers, the failure wording and the diagnostic text should not
// differ at all, so they live here instead of being copied into each reader.

// The headers Job Crush sends to any employer board.
//
// The User-Agent says who we are and links to the project. That is on purpose.
// A board owner who sees traffic they do not recognize should be able to look
// it up in one step and see a job search tool, not an anonymous scraper.
inline QNetworkRequest employerBoardRequest(const QUrl &requestUrl)
{
    QNetworkRequest networkRequest(requestUrl);
    networkRequest.setRawHeader(QByteArrayLiteral("Accept"),
                                QByteArrayLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("User-Agent"),
                                QByteArrayLiteral("Mozilla/5.0 (compatible; JobCrush/0.1; "
                                "+https://github.com/ultramanjones/jobcrush)"));
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                QNetworkRequest::NoLessSafeRedirectPolicy);
    return networkRequest;
}

// What a response actually was, when it turned out not to be job listings.
// Status, size and the opening bytes: enough to fix the problem from, without
// dumping a whole web page into a message a person has to read.
inline QString responseDiagnosticTail(int httpStatusCode, const QByteArray &responseBody)
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

// Fail a reply that was doomed before any request went out — an empty tenant,
// a link with no job id in it.
//
// The failure is queued rather than emitted right now so the caller gets the
// reply back and can connect to it first. Emitting before anyone is listening
// loses the message, and a lost failure looks exactly like a hang.
inline void failThisReplyOnceTheCallerIsListening(JobScoutReply *scoutReply,
                                                  const QString &humanReadableReason,
                                                  bool sourceHadTrouble = true)
{
    QMetaObject::invokeMethod(scoutReply,
                              [scoutReply, humanReadableReason, sourceHadTrouble]() {
        scoutReply->markFailed(humanReadableReason, sourceHadTrouble);
    }, Qt::QueuedConnection);
}

// The wording for the two failures every board reader hits.
// One voice across all of them, and every message ends with something to do.
inline QString boardDidNotAnswerMessage(const QString &boardDisplayName,
                                        const QString &networkErrorText,
                                        int httpStatusCode,
                                        const QByteArray &responseBody)
{
    return QStringLiteral("%1 didn't answer — %2. Check your connection and try again.%3")
        .arg(boardDisplayName, networkErrorText,
             responseDiagnosticTail(httpStatusCode, responseBody));
}

inline QString boardSentUnreadableMessage(const QString &boardDisplayName,
                                          int httpStatusCode,
                                          const QByteArray &responseBody)
{
    return QStringLiteral("%1 sent back something Job Crush couldn't read. "
                          "Try again in a few minutes.%2")
        .arg(boardDisplayName, responseDiagnosticTail(httpStatusCode, responseBody));
}

inline QString noSuchBoardMessage(const QString &boardDisplayName, const QString &tenant)
{
    return QStringLiteral("%1 has no job board called \"%2\". Check the spelling against "
                          "the job link, or paste the link and Job Crush will read the "
                          "name out of it.")
        .arg(boardDisplayName, tenant);
}

// Some boards never send the company's name. Lever does not, and neither does
// Ashby, so the board account name has to stand in for it.
//
// Capitalizing each word turns "acme-robotics" into "Acme Robotics", which
// reads like a company instead of a URL. It is not always right — "ibm"
// becomes "Ibm" — but a wrong capital is a smaller problem than a lowercase
// slug sitting where the employer's name belongs, and the user can edit it.
inline QString companyNameFromTenant(const QString &tenant)
{
    QString spacedName = tenant;
    spacedName.replace(QLatin1Char('-'), QLatin1Char(' '));
    spacedName.replace(QLatin1Char('_'), QLatin1Char(' '));

    const QStringList words = spacedName.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList capitalizedWords;
    capitalizedWords.reserve(words.size());
    for (const QString &word : words) {
        capitalizedWords.append(word.left(1).toUpper() + word.mid(1));
    }
    return capitalizedWords.join(QLatin1Char(' '));
}
