#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

// UrlCanonicalizer
//
// Turns a job URL into one stable form, so two links to the same job compare
// equal.
//
// The same posting reaches Job Crush by several routes, and each one adds its
// own tracking junk:
//
//   https://boards.greenhouse.io/acme/jobs/4055?gh_src=abc123
//   https://boards.greenhouse.io/acme/jobs/4055/?utm_source=linkedin&utm_medium=email
//   http://www.boards.greenhouse.io/acme/jobs/4055#app
//
// Those are one job. Without this, they are three rows on the board.
//
// What it does: lowercases the host, drops "www.", forces https, removes
// tracking parameters, drops the fragment, and removes a trailing slash. It
// keeps every other query parameter, because some boards put the job id there
// and dropping it would merge different jobs into one.
//
// No state, so it stays header-only.
class UrlCanonicalizer {
public:
    QString canonicalFormOf(const QString &rawUrl) const
    {
        const QString trimmedUrl = rawUrl.trimmed();
        if (trimmedUrl.isEmpty()) {
            return QString();
        }

        QUrl url(trimmedUrl);
        if (!url.isValid() || url.host().isEmpty()) {
            return trimmedUrl;   // not a URL we can take apart; hand it back as-is
        }

        // Every job board serves https. A stored http link would compare
        // unequal to the same job found later over https.
        url.setScheme(QStringLiteral("https"));

        QString host = url.host().toLower();
        if (host.startsWith(QStringLiteral("www."))) {
            host.remove(0, 4);
        }
        url.setHost(host);

        // The fragment is a scroll position, never part of the job's identity.
        url.setFragment(QString());
        url.setUserInfo(QString());

        QUrlQuery query(url);
        const QList<QPair<QString, QString>> everyParameter = query.queryItems();
        QUrlQuery keptQuery;
        for (const QPair<QString, QString> &parameter : everyParameter) {
            if (!parameterIsTracking(parameter.first)) {
                keptQuery.addQueryItem(parameter.first, parameter.second);
            }
        }
        url.setQuery(keptQuery);

        QString path = url.path();
        while (path.endsWith(QLatin1Char('/')) && path.length() > 1) {
            path.chop(1);
        }
        url.setPath(path);

        return url.toString();
    }

    // True when two URLs point at the same job.
    bool sameJob(const QString &firstUrl, const QString &secondUrl) const
    {
        const QString first = canonicalFormOf(firstUrl);
        const QString second = canonicalFormOf(secondUrl);
        return !first.isEmpty() && first.compare(second, Qt::CaseInsensitive) == 0;
    }

private:
    // Parameters that identify the CLICK, not the job.
    static bool parameterIsTracking(const QString &parameterName)
    {
        const QString name = parameterName.toLower();

        // Anything from a campaign tracker.
        if (name.startsWith(QStringLiteral("utm_"))) {
            return true;
        }

        static const QStringList trackingNames = {
            // ATS referral tags
            QStringLiteral("gh_src"), QStringLiteral("gh_jid"),
            QStringLiteral("lever-source"), QStringLiteral("lever-origin"),
            QStringLiteral("ashby_jid"),
            // aggregator and social referral tags
            QStringLiteral("ref"), QStringLiteral("referrer"), QStringLiteral("refid"),
            QStringLiteral("source"), QStringLiteral("src"), QStringLiteral("origin"),
            QStringLiteral("trk"), QStringLiteral("trackingid"), QStringLiteral("trk_ref"),
            QStringLiteral("li_fat_id"), QStringLiteral("originaljobid"),
            QStringLiteral("recommendedflavor"), QStringLiteral("position"),
            QStringLiteral("pagenum"), QStringLiteral("eboriginal"),
            // email and analytics tags
            QStringLiteral("mc_cid"), QStringLiteral("mc_eid"), QStringLiteral("fbclid"),
            QStringLiteral("gclid"), QStringLiteral("msclkid"), QStringLiteral("igshid"),
        };
        return trackingNames.contains(name);
    }
};
