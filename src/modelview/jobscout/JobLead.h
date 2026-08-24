#pragma once

#include <QDateTime>
#include <QString>

#include "AtsBoardIdentity.h"

// JobLead
//
// A job Job Crush has heard about but has not confirmed yet.
//
// Leads come from places that are not the employer: a forwarded LinkedIn
// alert, a recruiter email, a link the user pasted, a search API that returns
// a summary. What they have in common is that the text may be partial, stale,
// or a middleman's rewrite of the real posting.
//
// The difference between a lead and a JobPosting is trust. A JobPosting is
// what Job Crush is willing to show as the job. A lead is a starting point
// that still has to be resolved, and the two are kept apart on purpose: the
// day a stale aggregator summary gets saved as the real posting is the day the
// user applies to a job that closed a month ago.
//
// Pure data.
struct JobLead {
    // Whatever the lead came with. Any of these can be empty.
    QString positionTitle;
    QString companyName;
    QString locationText;
    QString salaryText;
    QDateTime postedTimestamp;
    bool isRemoteRole = false;

    // The link the lead arrived with. This may be a LinkedIn or aggregator
    // URL, which Job Crush keeps as a reference and does not fetch.
    QString discoveryUrl;

    // Where the lead came from: "linkedin-email", "pasted-url", "remotive",
    // and so on. Kept for the life of the job, because "where did this come
    // from?" is a question the user will ask.
    QString discoverySource;

    // The raw text the lead arrived with, when there was any: a pasted job
    // description, or the body of a forwarded email. Kept whole, because a
    // parse can be redone later and thrown-away text cannot.
    QString rawText;

    // Filled in by the resolver once it works out which hiring system this
    // job actually lives in. Empty until then.
    AtsBoardIdentity boardIdentity;

    // True when there is enough here to go looking for the real posting.
    bool hasSomethingToSearchOn() const
    {
        return !discoveryUrl.trimmed().isEmpty()
            || (!companyName.trimmed().isEmpty() && !positionTitle.trimmed().isEmpty());
    }
};
