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
// thing in the world. What separates them is only where they came from and
// whether the user has ever targeted one.
struct JobPosting {
    qint64 jobPostingId = 0;              // database identity; 0 means "not saved yet"
    QString companyName;
    QString positionTitle;
    QString locationText;                 // free text: "Remote", "Austin, TX", etc.
    QString salaryText;                   // free text as found in the posting, if any
    QString sourceUrl;                    // canonical link to the live posting
    QString fullDescriptionText;          // the complete posting text, tags stripped
    QDateTime discoveredTimestamp;        // when this posting entered Job Crush

    // --- The two source questions ---
    //
    // These answer different things and a job usually has an answer to both.
    // Keeping them apart is what lets the app show "you added this, and Job
    // Crush found the real posting on Ashby" — one sentence, two facts.

    // WHERE THE WORDS CAME FROM. The board or site this posting was read off:
    // "ashby", "lever", "greenhouse", "remotive", "jobicy", "usajobs". When
    // nothing could be read, this is empty and the posting holds only what the
    // user typed.
    //
    // This field used to be called discoverySource, which was wrong: it has
    // always held the board, never the finder.
    QString postingSource;

    // WHO BROUGHT IT IN. ScoutSourceText::You when the user pasted or typed
    // this job themselves, ScoutSourceText::Scout when a sweep turned it up.
    // A string rather than a flag, because more ways of getting jobs in are
    // coming and each deserves its own name.
    QString scoutSource;

    // --- Fields JobScout fills in; harmless and empty for manual entries ---

    // The source's OWN id for this job. Job Crush never invents one: paired
    // with postingSource it is what stops the same posting landing twice
    // when a sweep runs every day.
    QString externalSourceId;

    // When the EMPLOYER posted it (not when Job Crush found it). Freshness is
    // one of the strongest signals in a job search, so it earns its own field.
    QDateTime postedTimestamp;

    // Whether the source calls this a remote role. Free text in locationText
    // is too unreliable to answer this by parsing.
    bool isRemoteRole = false;

    // True when the user brought this job in by hand. The Manual Add tab shows
    // exactly these.
    bool wasAddedByHand() const;

    // True when the user brought it in and Job Crush could NOT find the real
    // posting on any board it is allowed to read. These are the rows that need
    // the user to fill in the rest themselves, and they say so.
    bool isManualAllTheWay() const;
};

// The two values scoutSource can hold, and the words for them.
//
// Stored as lowercase text so a person reading the database can read their own
// data, the same rule the pipeline stages follow.
namespace ScoutSourceText {

inline const QString You   = QStringLiteral("you");
inline const QString Scout = QStringLiteral("scout");

// What to put in front of a person. Anything unrecognized is shown as-is
// rather than guessed at.
inline QString displayNameFor(const QString &scoutSource)
{
    if (scoutSource == You) {
        return QStringLiteral("You");
    }
    if (scoutSource == Scout) {
        return QStringLiteral("Job Crush");
    }
    return scoutSource;
}

} // namespace ScoutSourceText

inline bool JobPosting::wasAddedByHand() const
{
    return scoutSource == ScoutSourceText::You;
}

inline bool JobPosting::isManualAllTheWay() const
{
    return wasAddedByHand() && postingSource.trimmed().isEmpty();
}
