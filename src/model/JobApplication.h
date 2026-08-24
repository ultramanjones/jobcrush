#pragma once

#include <QDateTime>
#include <QString>

#include "PipelineStage.h"

// JobApplication
//
// The user's campaign for one JobPosting. Born when the user hits CRUSH —
// this is what lives on the board and moves through the pipeline stages.
// Pure data, same rules as JobPosting.
struct JobApplication {
    qint64 jobApplicationId = 0;          // database identity; 0 means "not saved yet"
    qint64 jobPostingId = 0;              // the posting this campaign targets
    PipelineStage pipelineStage = PipelineStage::Saved;
    QDateTime targetedTimestamp;          // when the user crushed it onto the board
    QDateTime appliedTimestamp;           // when the human actually sent it (invalid until then)
    QString notesText;                    // the user's own running notes

    // How well the user's history matches this posting, 0-100, as scored by
    // the AI. -1 means it has not been scored. That is different from a score
    // of zero, and the two must stay distinguishable: an unscored job must not
    // sort below a job that scored badly.
    int fitScorePercent = -1;
};
