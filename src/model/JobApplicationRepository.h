#pragma once

#include <QList>
#include <QString>

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

    // Records WHEN the human actually sent the application. Separate from the
    // stage move that triggers it, because the stage is where the card sits
    // and this is a fact about the world.
    bool updateAppliedTimestamp(qint64 jobApplicationId, const QDateTime &appliedTimestamp);

    // Takes a campaign off the board. The posting it targeted is untouched.
    bool removeJobApplication(qint64 jobApplicationId);

    // Why the last write failed. Silence here is how a refused row turns into
    // an afternoon of guessing.
    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
