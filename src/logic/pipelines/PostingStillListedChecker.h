#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include "../jobscout/AtsBoardIdentity.h"
#include "PostingGoneConfirmer.h"

class JobPipelines;
class JobPostingRepository;
struct TargetedJob;

// PostingStillListedChecker
//
// Asks each employer's own board whether the jobs on the user's board are
// still listed.
//
// A job that drops off the employer's board has usually been filled or
// pulled. The user should know that before spending an evening on a cover
// letter. Nothing is deleted: the card is flagged, it says so in plain words,
// and the user decides what to do.
//
// Each job is checked on every board link Job Crush has for it: the job's own
// link first, then any other place it was found. A job is marked gone only
// when every link that could be checked says gone, and each of those answers
// is itself two answers that agree (see PostingGoneConfirmer). If any link
// says the job is still listed, it is still listed.
//
// Only links to Greenhouse, Lever, Ashby, SmartRecruiters, Workday and Taleo can be
// checked. A job with none of these is counted as "no link Job Crush can
// check" and left alone.
//
// A job the user marked "still open, I checked" is not checked. The user's
// word wins.
//
// It runs when the user asks, never on its own. Job Crush does not sit in the
// background calling other people's servers.
//
// Cards in Closed are skipped. That search is over.
class PostingStillListedChecker : public QObject {
    Q_OBJECT
public:
    PostingStillListedChecker(JobPipelines &pipelines,
                              JobPostingRepository &postingRepository,
                              QObject *parent = nullptr);

    // Starts a check of every open card. Does nothing if one is running.
    void checkEveryOpenCard();

    bool checkIsRunning() const;

    // What is happening, in real numbers: "Checked 3 of 7 jobs…". After the
    // check, what was found. Empty before the first check.
    QString checkStatusText() const;

signals:
    void checkStatusChanged();

private:
    // Every board link for one card that Job Crush can check, the job's own
    // link first. The same job is never listed twice.
    QList<AtsBoardIdentity> checkableLinksFor(const TargetedJob &targetedJob) const;

    // One link answered for one job.
    void recordLinkAnswer(qint64 jobPostingId, ListingAnswer listingAnswer);

    // Every link for one job has answered. Decides the job's answer and
    // saves it.
    void finishOneJob(qint64 jobPostingId);

    // Builds the closing sentence once every job is done.
    void finishTheCheck();

    // What the links said so far for one job.
    struct JobBeingChecked {
        QString positionTitle;
        bool wasAlreadyGone = false;
        int linksStillToAnswer = 0;
        int linksStillListed = 0;
        int linksGone = 0;
        int linksCouldNotCheck = 0;
    };

    JobPipelines &board;
    JobPostingRepository &jobPostingRepository;
    PostingGoneConfirmer goneConfirmer;

    QHash<qint64, JobBeingChecked> jobsBeingChecked;

    bool checkRunning = false;

    // Jobs sent off to be checked, and jobs whose every link has answered.
    // "Checked 3 of 7" is checksFinished of checksStarted.
    int checksStarted = 0;
    int checksFinished = 0;

    // What the check found, one count per outcome, for the closing sentence.
    int jobsStillListed = 0;
    int jobsNewlyGone = 0;
    int jobsStillGone = 0;
    int jobsThatCouldNotBeChecked = 0;
    int jobsWithNoReadableLink = 0;
    int jobsTheUserSaidAreOpen = 0;
    QString statusSentence;
};
