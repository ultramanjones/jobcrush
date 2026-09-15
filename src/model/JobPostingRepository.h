#pragma once

#include <QList>
#include <QString>

#include "JobPosting.h"

class JobCrushDatabase;

// JobPostingRepository
//
// The model-layer gateway for JobPosting rows. Everything above this class
// speaks in JobPosting structs; only this class speaks SQL.
class JobPostingRepository {
public:
    explicit JobPostingRepository(JobCrushDatabase &database);

    // Saves a new posting and returns it with jobPostingId filled in.
    // Returns false on database failure.
    bool insertJobPosting(JobPosting &jobPosting);

    // Saves a JobScout discovery unless this source has already delivered
    // this exact job before. wasAlreadyKnown reports which happened, so a
    // sweep can honestly say "14 new, 61 already seen" instead of pretending
    // everything it fetched was a find.
    //
    // Returns false only on a real database failure — a duplicate is an
    // ordinary outcome, not an error.
    bool insertDiscoveryIfNew(JobPosting &jobPosting, bool &wasAlreadyKnown);

    // Every posting known to Job Crush, newest first.
    QList<JobPosting> loadAllJobPostings();

    // Everything read off one board or site, newest posting first — this is
    // what fills that source's tab on the Discoveries page.
    QList<JobPosting> loadJobPostingsFromSource(const QString &postingSource);

    // Moves a job Job Crush already had over to the hand-added list.
    //
    // A sweep often turns a job up before the user ever goes looking, and then
    // the user pastes the same job in by hand. That paste is still them
    // bringing it in — they went and got the link. Without this, the job stays
    // filed as a sweep find and never appears on the tab they were told to
    // look at, which is the same disappearing act this tab exists to end.
    bool markJobPostingAsHandAdded(qint64 jobPostingId);

    // Every job the user brought in by hand, newest ADDED first. This fills
    // the Manual Add tab, and the ordering is deliberate: the job you just
    // pasted has to be the first row, whatever date the employer posted it.
    QList<JobPosting> loadHandAddedJobPostings();

    // Everything JobScout has ever found, from every source, newest first.
    // Top Prospects ranks this list rather than any single source's.
    QList<JobPosting> loadAllDiscoveredJobPostings();

    // One posting by id. found is set accordingly.
    JobPosting loadJobPostingById(qint64 jobPostingId, bool &found);

    // Why the last insert or query failed. A repository that returns false
    // and keeps the reason to itself forces every caller to guess.
    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
