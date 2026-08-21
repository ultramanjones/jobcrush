#pragma once

#include <QList>

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

    // Every posting known to Job Crush, newest first.
    QList<JobPosting> loadAllJobPostings();

    // One posting by id. found is set accordingly.
    JobPosting loadJobPostingById(qint64 jobPostingId, bool &found);

private:
    JobCrushDatabase &jobCrushDatabase;
};
