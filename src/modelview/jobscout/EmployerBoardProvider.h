#pragma once

#include <QString>

#include "AtsBoardIdentity.h"

class JobScoutReply;
class QObject;

// EmployerBoardProvider
//
// The interface for reading ONE employer's own job board.
//
// This is the other half of job discovery, and it is a different job from
// JobSourceProvider. A JobSourceProvider searches a whole site: "find me C++
// jobs". An EmployerBoardProvider fetches everything one company has posted on
// one hiring system: "show me every open job at Acme on Greenhouse".
//
// Why both exist:
//
//   Search APIs find employers you had not heard of. They also hand back
//   whatever text the middleman kept, which may be a summary, may be stale,
//   and may be missing the parts that matter.
//
//   Employer boards give the real posting: the employer's own words, the whole
//   description, and a job that disappears when it is filled. But you have to
//   know which employer to ask.
//
// So search finds the lead, and the employer board confirms it. That is the
// canonical resolver, and it is why a lead is kept apart from a posting.
//
// Same rules as JobSourceProvider: a thin HTTPS client over a documented
// public endpoint, no state, no database, no scoring. Job Crush does not
// scrape sites that forbid it.
class EmployerBoardProvider {
public:
    virtual ~EmployerBoardProvider() = default;

    // Which hiring system this reads, from AtsBoardName.
    virtual QString boardName() const = 0;

    // Every published job for one employer account. tenant is the employer's
    // name on that board: the board token, site name, or company slug.
    virtual JobScoutReply *fetchEveryJobForEmployer(const QString &tenant,
                                                    QObject *replyParent) = 0;

    // One job. The reply carries a list so the shape matches the sweep above;
    // it holds one posting on success and none when that job is gone, which is
    // itself worth knowing — a job that has closed should stop looking open.
    virtual JobScoutReply *fetchOneJob(const AtsBoardIdentity &boardIdentity,
                                       QObject *replyParent) = 0;
};
