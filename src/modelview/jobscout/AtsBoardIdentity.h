#pragma once

#include <QString>

// AtsBoardName
//
// The hiring systems Job Crush can read directly. An employer using one of
// these publishes their jobs through a documented public endpoint, so Job
// Crush can fetch the real posting instead of whatever an aggregator kept.
namespace AtsBoardName {

inline const QString Greenhouse     = QStringLiteral("greenhouse");
inline const QString Lever          = QStringLiteral("lever");
inline const QString Ashby          = QStringLiteral("ashby");
inline const QString SmartRecruiters = QStringLiteral("smartrecruiters");
inline const QString Workable       = QStringLiteral("workable");
inline const QString Recruitee      = QStringLiteral("recruitee");
inline const QString Personio       = QStringLiteral("personio");

inline QString displayNameFor(const QString &boardName)
{
    if (boardName == Greenhouse)      return QStringLiteral("Greenhouse");
    if (boardName == Lever)           return QStringLiteral("Lever");
    if (boardName == Ashby)           return QStringLiteral("Ashby");
    if (boardName == SmartRecruiters) return QStringLiteral("SmartRecruiters");
    if (boardName == Workable)        return QStringLiteral("Workable");
    if (boardName == Recruitee)       return QStringLiteral("Recruitee");
    if (boardName == Personio)        return QStringLiteral("Personio");
    return QString();
}

} // namespace AtsBoardName

// AtsBoardIdentity
//
// Which hiring system a job lives in, whose account it is, and which job.
//
// This is the strongest identity a posting can have. Two leads that resolve to
// the same board, tenant and job id are the same job, no matter how different
// their titles or URLs look. Nothing else Job Crush can compute comes close.
//
// tenant is the employer's account name on that board: the part of the URL
// that says which company. Greenhouse calls it a board token, Lever calls it a
// site, Ashby calls it a job board name. Same idea, so one name here.
struct AtsBoardIdentity {
    QString boardName;   // one of AtsBoardName, or empty when not recognized
    QString tenant;      // the employer's account on that board
    QString jobId;       // the board's own id for this job, when the URL has one

    bool isKnown() const { return !boardName.isEmpty() && !tenant.isEmpty(); }

    // True when this names one specific job rather than just an employer.
    bool namesOneJob() const { return isKnown() && !jobId.isEmpty(); }

    // "greenhouse/acme/4055" — for logging and for a dedupe key.
    QString asKey() const
    {
        if (!isKnown()) {
            return QString();
        }
        return jobId.isEmpty()
            ? QStringLiteral("%1/%2").arg(boardName, tenant)
            : QStringLiteral("%1/%2/%3").arg(boardName, tenant, jobId);
    }
};
