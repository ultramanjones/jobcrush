#pragma once

#include <QDateTime>
#include <QString>

// JobPosting
//
// A job that exists out in the world. Pure data — no behavior, no Qt Quick,
// no knowledge of anything above it. A posting on its own is NOT on the board;
// only a JobApplication (created when the user hits CRUSH) puts it there.
struct JobPosting {
    qint64 jobPostingId = 0;              // database identity; 0 means "not saved yet"
    QString companyName;
    QString positionTitle;
    QString locationText;                 // free text: "Remote", "Austin, TX", etc.
    QString salaryText;                   // free text as found in the posting, if any
    QString sourceUrl;                    // canonical link to the live posting
    QString fullDescriptionText;          // the complete posting text
    QString discoverySource;              // "manual" today; JobScout source names later
    QDateTime discoveredTimestamp;        // when this posting entered Job Crush
};
