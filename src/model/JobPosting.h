#pragma once

#include <QDateTime>
#include <QString>

// JobPosting
//
// A job that exists out in the world. Pure data — no behavior, no Qt Quick,
// no knowledge of anything above it. A posting on its own is NOT on the board;
// only a JobApplication (created when the user hits CRUSH) puts it there.
//
// A JobScout discovery IS one of these — there is deliberately no separate
// "discovery" entity, because a found job and a hand-entered job are the same
// thing in the world. What separates them is only where they came from
// (discoverySource) and whether the user has ever targeted one.
struct JobPosting {
    qint64 jobPostingId = 0;              // database identity; 0 means "not saved yet"
    QString companyName;
    QString positionTitle;
    QString locationText;                 // free text: "Remote", "Austin, TX", etc.
    QString salaryText;                   // free text as found in the posting, if any
    QString sourceUrl;                    // canonical link to the live posting
    QString fullDescriptionText;          // the complete posting text, tags stripped
    QString discoverySource;              // "manual", or a JobScout source name
    QDateTime discoveredTimestamp;        // when this posting entered Job Crush

    // --- Fields JobScout fills in; harmless and empty for manual entries ---

    // The source's OWN id for this job. Job Crush never invents one: paired
    // with discoverySource it is what stops the same posting landing twice
    // when a sweep runs every day.
    QString externalSourceId;

    // When the EMPLOYER posted it (not when Job Crush found it). Freshness is
    // one of the strongest signals in a job search, so it earns its own field.
    QDateTime postedTimestamp;

    // Whether the source calls this a remote role. Free text in locationText
    // is too unreliable to answer this by parsing.
    bool isRemoteRole = false;
};
