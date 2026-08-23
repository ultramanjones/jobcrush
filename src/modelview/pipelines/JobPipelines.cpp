#include "JobPipelines.h"

#include <QDateTime>

#include "../../model/JobApplicationRepository.h"
#include "../../model/JobPostingRepository.h"

JobPipelines::JobPipelines(JobApplicationRepository &applicationRepository,
                           JobPostingRepository &postingRepositoryToUse,
                           QObject *parent)
    : QObject(parent)
    , campaignRepository(applicationRepository)
    , postingRepository(postingRepositoryToUse)
{
}

void JobPipelines::loadFromDatabase()
{
    loadedTargetedJobs.clear();

    for (const JobApplication &campaign : campaignRepository.loadAllJobApplications()) {
        bool postingFound = false;
        const JobPosting posting =
            postingRepository.loadJobPostingById(campaign.jobPostingId, postingFound);

        // A campaign whose posting has vanished is a card with no job on it.
        // Skipping it keeps the board honest; it is not deleted, because the
        // row is evidence that something went wrong and deleting evidence
        // makes the next bug harder to find.
        if (!postingFound) {
            continue;
        }
        loadedTargetedJobs.append({ campaign, posting });
    }

    emit boardChanged();
}

QList<TargetedJob> JobPipelines::everyTargetedJob() const
{
    return loadedTargetedJobs;
}

int JobPipelines::countInStage(PipelineStage pipelineStage) const
{
    int cardCount = 0;
    for (const TargetedJob &targetedJob : loadedTargetedJobs) {
        if (targetedJob.campaign.pipelineStage == pipelineStage) {
            ++cardCount;
        }
    }
    return cardCount;
}

bool JobPipelines::jobPostingIsOnTheBoard(qint64 jobPostingId) const
{
    for (const TargetedJob &targetedJob : loadedTargetedJobs) {
        if (targetedJob.campaign.jobPostingId == jobPostingId) {
            return true;
        }
    }
    return false;
}

bool JobPipelines::crushJobPosting(qint64 jobPostingId, QString &reasonText)
{
    bool postingFound = false;
    const JobPosting posting = postingRepository.loadJobPostingById(jobPostingId, postingFound);
    if (!postingFound) {
        reasonText = QStringLiteral(
            "That job isn't in Job Crush any more. Run a sweep on Discoveries and "
            "it should come back — job boards do take postings down.");
        return false;
    }

    if (jobPostingIsOnTheBoard(jobPostingId)) {
        reasonText = QStringLiteral("%1 at %2 is already on your board.")
            .arg(posting.positionTitle, posting.companyName);
        return false;
    }

    JobApplication newCampaign;
    newCampaign.jobPostingId = jobPostingId;
    newCampaign.pipelineStage = PipelineStage::Saved;
    newCampaign.targetedTimestamp = QDateTime::currentDateTime();

    if (!campaignRepository.insertJobApplication(newCampaign)) {
        // The database's own words, because "try again in a moment" is what an
        // app says when it has not bothered to find out.
        reasonText = QStringLiteral(
            "Job Crush couldn't put that on the board — %1. The job is still on "
            "Discoveries, so nothing was lost.")
                .arg(campaignRepository.lastErrorText());
        return false;
    }

    loadFromDatabase();
    // Deliberately does NOT name the column. "Saved" is what the stage is
    // called in the database; "Crushed" is what it says on the board. This
    // layer knows the first and has no business asserting the second, and a
    // sentence naming a column the user cannot find is worse than one that
    // does not try.
    reasonText = QStringLiteral("%1 at %2 is on your board.")
        .arg(posting.positionTitle, posting.companyName);
    return true;
}

bool JobPipelines::moveToStage(qint64 jobApplicationId, PipelineStage newPipelineStage)
{
    if (!campaignRepository.updatePipelineStage(jobApplicationId, newPipelineStage)) {
        return false;
    }

    // Arriving at Applied stamps the date, once. "When did I apply?" is the
    // question every job search fails to answer three weeks later, and the
    // only moment anyone reliably knows the answer is the moment they drag
    // the card across.
    //
    // Dragging back out does NOT clear it. You applied on the 3rd; moving the
    // card somewhere else does not make that untrue, and erasing the date
    // would destroy the one fact the board was keeping for you.
    if (newPipelineStage == PipelineStage::Applied) {
        for (const TargetedJob &targetedJob : loadedTargetedJobs) {
            if (targetedJob.campaign.jobApplicationId == jobApplicationId
                    && !targetedJob.campaign.appliedTimestamp.isValid()) {
                campaignRepository.updateAppliedTimestamp(jobApplicationId,
                                                          QDateTime::currentDateTime());
                break;
            }
        }
    }

    loadFromDatabase();
    return true;
}

bool JobPipelines::setNotesText(qint64 jobApplicationId, const QString &notesText)
{
    if (!campaignRepository.updateNotesText(jobApplicationId, notesText)) {
        return false;
    }
    loadFromDatabase();
    return true;
}

bool JobPipelines::removeFromBoard(qint64 jobApplicationId)
{
    if (!campaignRepository.removeJobApplication(jobApplicationId)) {
        return false;
    }
    loadFromDatabase();
    return true;
}
