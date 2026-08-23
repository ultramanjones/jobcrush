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

    // Everything one JobScout source has delivered, newest posting first —
    // this is what fills that source's tab on the Discoveries page.
    QList<JobPosting> loadJobPostingsFromSource(const QString &discoverySource);

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
