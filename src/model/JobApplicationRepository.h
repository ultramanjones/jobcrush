#pragma once

#include <QList>

#include "JobApplication.h"

class JobCrushDatabase;

// JobApplicationRepository
//
// The model-layer gateway for JobApplication rows — the campaigns that live
// on the board. Same rules as JobPostingRepository: structs in, structs out,
// SQL stays inside.
class JobApplicationRepository {
public:
    explicit JobApplicationRepository(JobCrushDatabase &database);

    // Saves a new application (a CRUSH!) and returns it with
    // jobApplicationId filled in. Returns false on database failure.
    bool insertJobApplication(JobApplication &jobApplication);

    // Every campaign, oldest first (stable board order).
    QList<JobApplication> loadAllJobApplications();

    // Moves a campaign to a new pipeline stage.
    bool updatePipelineStage(qint64 jobApplicationId, PipelineStage newPipelineStage);

    // Replaces the user's notes for a campaign.
    bool updateNotesText(qint64 jobApplicationId, const QString &newNotesText);

private:
    JobCrushDatabase &jobCrushDatabase;
};
