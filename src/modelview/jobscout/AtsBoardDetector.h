#pragma once

#include <QRegularExpression>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

#include "AtsBoardIdentity.h"

// AtsBoardDetector
//
// Reads a job URL and works out which hiring system it belongs to, which
// employer, and which job.
//
// This is what turns a link into something Job Crush can fetch for real. A
// LinkedIn alert, a recruiter email, and an aggregator all hand over a URL.
// If that URL points at Greenhouse, Job Crush can ask Greenhouse for the
// posting and get the employer's own words, the full description, and the
// current status, instead of whatever text the middleman kept.
//
// It only reads the URL. It makes no network request and guesses nothing: a
// URL it does not recognize comes back empty, and the caller falls back to
// asking the user.
//
// No state, so it stays header-only.
class AtsBoardDetector {
public:
    AtsBoardIdentity identify(const QString &jobUrl) const
    {
        AtsBoardIdentity identity;

        const QUrl url(jobUrl.trimmed());
        if (!url.isValid() || url.host().isEmpty()) {
            return identity;
        }

        QString host = url.host().toLower();
        if (host.startsWith(QStringLiteral("www."))) {
            host.remove(0, 4);
        }
        const QStringList pathParts =
            url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        const QUrlQuery query(url);

        // --- Greenhouse ---------------------------------------------------
        // boards.greenhouse.io/acme/jobs/4055
        // job-boards.greenhouse.io/acme/jobs/4055
        // job-boards.eu.greenhouse.io/acme/jobs/4055   (EU-hosted boards)
        // acme.greenhouse.io/jobs/4055                 (rare, older embed)
        if (host.endsWith(QStringLiteral("greenhouse.io"))) {
            identity.boardName = AtsBoardName::Greenhouse;

            // The company is the first thing in the PATH on every modern
            // Greenhouse link, whatever the host in front of it. Listing the
            // hosts instead was wrong the moment Greenhouse added an EU one:
            // job-boards.eu.greenhouse.io fell through to "first bit of the
            // host", and Job Crush went looking for a company called
            // "job-boards".
            //
            // The old embed is the exception, and it says so: its path starts
            // with "jobs", so the company has to come from the host.
            const bool pathStartsWithTheCompany =
                !pathParts.isEmpty()
                && pathParts.at(0) != QStringLiteral("jobs")
                && pathParts.at(0) != QStringLiteral("embed");

            identity.tenant = pathStartsWithTheCompany
                ? pathParts.at(0)
                : host.section(QLatin1Char('.'), 0, 0);
            identity.jobId = partAfter(pathParts, QStringLiteral("jobs"));
            // Some links carry the job id only as a parameter.
            if (identity.jobId.isEmpty()) {
                identity.jobId = query.queryItemValue(QStringLiteral("gh_jid"));
            }
            return identity;
        }

        // --- Lever --------------------------------------------------------
        // jobs.lever.co/acme/7f3a1b2c-...
        // jobs.eu.lever.co/acme/7f3a1b2c-...
        if (host.endsWith(QStringLiteral("lever.co"))) {
            identity.boardName = AtsBoardName::Lever;
            if (!pathParts.isEmpty()) {
                identity.tenant = pathParts.at(0);
            }
            if (pathParts.count() > 1) {
                identity.jobId = pathParts.at(1);
            }
            return identity;
        }

        // --- Ashby --------------------------------------------------------
        // jobs.ashbyhq.com/acme/7f3a1b2c-...
        if (host.endsWith(QStringLiteral("ashbyhq.com"))) {
            identity.boardName = AtsBoardName::Ashby;
            if (!pathParts.isEmpty()) {
                identity.tenant = pathParts.at(0);
            }
            if (pathParts.count() > 1) {
                identity.jobId = pathParts.at(1);
            }
            if (identity.jobId.isEmpty()) {
                identity.jobId = query.queryItemValue(QStringLiteral("ashby_jid"));
            }
            return identity;
        }

        // --- SmartRecruiters ----------------------------------------------
        // jobs.smartrecruiters.com/Acme/744000012345-engineer
        // careers.smartrecruiters.com/Acme/...
        if (host.endsWith(QStringLiteral("smartrecruiters.com"))) {
            identity.boardName = AtsBoardName::SmartRecruiters;
            if (!pathParts.isEmpty()) {
                identity.tenant = pathParts.at(0);
            }
            if (pathParts.count() > 1) {
                // The id is the leading digits of the slug: "744000012345-title".
                static const QRegularExpression leadingDigitsPattern(
                    QStringLiteral("^(\\d{6,})"));
                const QRegularExpressionMatch match =
                    leadingDigitsPattern.match(pathParts.at(1));
                identity.jobId = match.hasMatch() ? match.captured(1) : pathParts.at(1);
            }
            return identity;
        }

        // --- Workable -----------------------------------------------------
        // apply.workable.com/acme/j/ABC123DEF/
        // acme.workable.com/j/ABC123DEF
        if (host.endsWith(QStringLiteral("workable.com"))) {
            identity.boardName = AtsBoardName::Workable;
            if (host == QStringLiteral("apply.workable.com")
                    || host == QStringLiteral("jobs.workable.com")) {
                if (!pathParts.isEmpty()) {
                    identity.tenant = pathParts.at(0);
                }
            } else {
                identity.tenant = host.section(QLatin1Char('.'), 0, 0);
            }
            identity.jobId = partAfter(pathParts, QStringLiteral("j"));
            return identity;
        }

        // --- Recruitee ----------------------------------------------------
        // acme.recruitee.com/o/senior-engineer
        if (host.endsWith(QStringLiteral("recruitee.com"))) {
            identity.boardName = AtsBoardName::Recruitee;
            identity.tenant = host.section(QLatin1Char('.'), 0, 0);
            identity.jobId = partAfter(pathParts, QStringLiteral("o"));
            return identity;
        }

        // --- Personio -----------------------------------------------------
        // acme.jobs.personio.de/job/123456
        // acme.jobs.personio.com/job/123456
        if (host.contains(QStringLiteral("personio."))) {
            identity.boardName = AtsBoardName::Personio;
            identity.tenant = host.section(QLatin1Char('.'), 0, 0);
            identity.jobId = partAfter(pathParts, QStringLiteral("job"));
            return identity;
        }

        return identity;   // not a board Job Crush knows
    }

    // True when the URL is a walled garden Job Crush must not fetch. Those
    // links are kept as a reference and resolved through their employer's own
    // board instead. See the LinkedIn and Indeed sections of the plan.
    bool isWalledGarden(const QString &jobUrl) const
    {
        const QUrl url(jobUrl.trimmed());
        QString host = url.host().toLower();
        if (host.startsWith(QStringLiteral("www."))) {
            host.remove(0, 4);
        }
        static const QStringList walledHosts = {
            QStringLiteral("linkedin.com"), QStringLiteral("indeed.com"),
            QStringLiteral("glassdoor.com"), QStringLiteral("ziprecruiter.com"),
            QStringLiteral("monster.com"), QStringLiteral("dice.com"),
            QStringLiteral("simplyhired.com"), QStringLiteral("careerbuilder.com"),
        };
        for (const QString &walledHost : walledHosts) {
            if (host == walledHost || host.endsWith(QLatin1Char('.') + walledHost)) {
                return true;
            }
        }
        return false;
    }

private:
    // The path part that follows a given marker: partAfter([acme,jobs,4055],
    // "jobs") is "4055".
    static QString partAfter(const QStringList &pathParts, const QString &marker)
    {
        for (int partIndex = 0; partIndex + 1 < pathParts.count(); ++partIndex) {
            if (pathParts.at(partIndex).compare(marker, Qt::CaseInsensitive) == 0) {
                return pathParts.at(partIndex + 1);
            }
        }
        return QString();
    }
};
