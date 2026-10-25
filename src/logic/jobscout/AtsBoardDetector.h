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
    // How it works: the host says which hiring system it is. Each system has
    // its own section below, with the link shapes it accepts written above
    // it. Inside a section the link is cut into its path parts, and the
    // company and the job id are picked out by position or by the word that
    // comes before them. The first section whose host matches wins.
    AtsBoardIdentity identify(const QString &jobUrl) const
    {
        AtsBoardIdentity identity;

        const QUrl url(jobUrl.trimmed());
        if (!url.isValid() || url.host().isEmpty()) {
            return identity;
        }

        // "www.lever.co" and "lever.co" are the same host.
        const QString wwwPrefix = QStringLiteral("www.");
        QString host = url.host().toLower();
        if (host.startsWith(wwwPrefix)) {
            host.remove(0, wwwPrefix.size());
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
            // with "jobs" (or "embed"), so the company has to come from the
            // host.
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
        // The first path part is the company and the second is the job id.
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
        // jobs.ashbyhq.com/acme?ashby_jid=7f3a1b2c-...  (id as a parameter)
        //
        // Same shape as Lever: company first, job id second. Some links
        // carry the id only as the ashby_jid parameter.
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
                // The pattern reads: from the start (^), six or more digits
                // (\d{6,}), kept as group 1. Six is the floor so a title that
                // starts with a short number, like "3d-artist", is not taken
                // for an id. With no match, the whole part is used as the id.
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
        //
        // On the shared hosts the company is the first path part. On a
        // company's own subdomain it is the first part of the host. Either
        // way the job id is the path part after "j".
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
        //
        // Company from the host, job from the path part after "o".
        if (host.endsWith(QStringLiteral("recruitee.com"))) {
            identity.boardName = AtsBoardName::Recruitee;
            identity.tenant = host.section(QLatin1Char('.'), 0, 0);
            identity.jobId = partAfter(pathParts, QStringLiteral("o"));
            return identity;
        }

        // --- Personio -----------------------------------------------------
        // acme.jobs.personio.de/job/123456
        // acme.jobs.personio.com/job/123456
        //
        // Personio has a host per country, so the test is "personio." anywhere
        // in the host. Company from the host, job from the part after "job".
        if (host.contains(QStringLiteral("personio."))) {
            identity.boardName = AtsBoardName::Personio;
            identity.tenant = host.section(QLatin1Char('.'), 0, 0);
            identity.jobId = partAfter(pathParts, QStringLiteral("job"));
            return identity;
        }

        // --- Workday -----------------------------------------------------
        // ngc.wd1.myworkdayjobs.com/en-US/Northrop_Grumman_External_Site/job/
        //     United-States-Georgia-Warner-Robins/Embedded---Principal-Software-Engineer_R10252946
        // ngc.wd1.myworkdayjobs.com/Northrop_Grumman_External_Site/job/...   (no language)
        //
        // Workday needs three parts to find a job: the account (ngc), the
        // server it lives on (wd1), and the careers site (the path part after
        // the optional language). They are kept together in the tenant as
        // "ngc.wd1/Northrop_Grumman_External_Site". The job id is the rest of
        // the path from "job" on, because that whole path is how Workday
        // finds one job.
        //
        // Steps:
        //   1. Split the host on dots. "ngc.wd1.myworkdayjobs.com" is four
        //      parts: account, server, "myworkdayjobs", "com". Fewer than four
        //      means the account or the server is missing, so give up.
        //   2. Skip a language part like "en-US" at the front of the path.
        //   3. The next path part is the careers site. No site, give up.
        //   4. If the part after the site is "job" and something follows it,
        //      everything from "job" on is the job id.
        if (host.endsWith(QStringLiteral(".myworkdayjobs.com"))) {
            constexpr int fewestHostPartsWithAccountAndServer = 4;
            const QStringList hostParts = host.split(QLatin1Char('.'));
            if (hostParts.count() < fewestHostPartsWithAccountAndServer) {
                return identity;
            }
            int pathIndex = 0;
            // Two lowercase letters, a dash, two letters: "en-US", "fr-fr".
            static const QRegularExpression languagePattern(
                QStringLiteral("^[a-z]{2}-[A-Za-z]{2}$"));
            if (pathIndex < pathParts.count()
                    && languagePattern.match(pathParts.at(pathIndex)).hasMatch()) {
                ++pathIndex;
            }
            if (pathIndex >= pathParts.count()) {
                return identity;
            }
            identity.boardName = AtsBoardName::Workday;
            identity.tenant = QStringLiteral("%1.%2/%3")
                .arg(hostParts.at(0), hostParts.at(1), pathParts.at(pathIndex));
            ++pathIndex;
            if (pathIndex < pathParts.count()
                    && pathParts.at(pathIndex).compare(QStringLiteral("job"),
                                                       Qt::CaseInsensitive) == 0
                    && pathIndex + 1 < pathParts.count()) {
                identity.jobId = pathParts.mid(pathIndex).join(QLatin1Char('/'));
            }
            return identity;
        }

        // --- Workday, the other host ------------------------------------
        // wd5.myworkdaysite.com/en-US/recruiting/uw/UWHires/job/Seattle-WA/Title_REQ-0000136398
        //
        // Here the account (uw) and the careers site (UWHires) are in the
        // path, after "recruiting". They are kept as
        // "wd5.myworkdaysite/uw/UWHires", so WorkdayBoardSource can tell this
        // host from the usual one.
        //
        // Steps:
        //   1. The server name is the first part of the host: "wd5".
        //   2. Skip a language part like "en-US" at the front of the path.
        //   3. The next part must be "recruiting", with at least two parts
        //      after it: the account, then the site. If not, give up.
        //   4. If the part after the site is "job" and something follows it,
        //      everything from "job" on is the job id.
        if (host.endsWith(QStringLiteral(".myworkdaysite.com"))) {
            const QString serverName = host.section(QLatin1Char('.'), 0, 0);
            int pathIndex = 0;
            // Same language pattern as above: "en-US".
            static const QRegularExpression siteLanguagePattern(
                QStringLiteral("^[a-z]{2}-[A-Za-z]{2}$"));
            if (pathIndex < pathParts.count()
                    && siteLanguagePattern.match(pathParts.at(pathIndex)).hasMatch()) {
                ++pathIndex;
            }
            if (pathIndex + 2 >= pathParts.count()
                    || pathParts.at(pathIndex).compare(QStringLiteral("recruiting"),
                                                       Qt::CaseInsensitive) != 0) {
                return identity;
            }
            identity.boardName = AtsBoardName::Workday;
            identity.tenant = QStringLiteral("%1.myworkdaysite/%2/%3")
                .arg(serverName, pathParts.at(pathIndex + 1), pathParts.at(pathIndex + 2));
            pathIndex += 3;
            if (pathIndex < pathParts.count()
                    && pathParts.at(pathIndex).compare(QStringLiteral("job"),
                                                       Qt::CaseInsensitive) == 0
                    && pathIndex + 1 < pathParts.count()) {
                identity.jobId = pathParts.mid(pathIndex).join(QLatin1Char('/'));
            }
            return identity;
        }

        // --- Taleo --------------------------------------------------------
        // hdr.taleo.net/careersection/ex/jobdetail.ftl?job=193686&lang=en
        // hdr.taleo.net/careersection/ex/jobsearch.ftl?lang=en   (the site, no job)
        //
        // The account is the first part of the host, and the careers section
        // is the part after "careersection". They are kept as "hdr/ex". The
        // job number is the "job" setting in the link.
        //
        // "careersection/rest/..." is Taleo's own data service, not a careers
        // section a person opens, so a link with "rest" there is not taken.
        if (host.endsWith(QStringLiteral(".taleo.net"))) {
            const int sectionIndex = pathParts.indexOf(QStringLiteral("careersection"));
            if (sectionIndex < 0 || sectionIndex + 1 >= pathParts.count()
                    || pathParts.at(sectionIndex + 1) == QStringLiteral("rest")) {
                return identity;
            }
            identity.boardName = AtsBoardName::Taleo;
            identity.tenant = QStringLiteral("%1/%2").arg(host.section(QLatin1Char('.'), 0, 0),
                                                          pathParts.at(sectionIndex + 1));
            identity.jobId = query.queryItemValue(QStringLiteral("job"));
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
        const QString wwwPrefix = QStringLiteral("www.");
        QString host = url.host().toLower();
        if (host.startsWith(wwwPrefix)) {
            host.remove(0, wwwPrefix.size());
        }
        // A host matches when it is the site itself or any subdomain of it:
        // "linkedin.com" and "uk.linkedin.com" both count.
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
