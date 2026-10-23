#pragma once

#include <QObject>
#include <QString>

#include "../jobscout/AshbyBoardSource.h"
#include "../jobscout/AtsBoardIdentity.h"
#include "../jobscout/GreenhouseBoardSource.h"
#include "../jobscout/LeverBoardSource.h"
#include "../jobscout/SmartRecruitersBoardSource.h"
#include "../jobscout/WorkdayBoardSource.h"

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
// Only jobs with a link to a board Job Crush can read are checked: Greenhouse,
// Lever, Ashby and Workday. Anything else is counted as "could not check" and
// left alone.
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
    // The board and job to ask about for one card, or an unknown identity
    // when the card has no link Job Crush can read.
    AtsBoardIdentity readableIdentityFor(const TargetedJob &targetedJob) const;
    EmployerBoardProvider *boardReaderFor(const QString &boardName);

    void recordAnswer(qint64 jobPostingId, bool postingIsListed, bool wasAlreadyGone);
    void recordTrouble(const QString &positionTitle, const QString &whyItFailed);
    void finishOneCheck();

    JobPipelines &board;
    JobPostingRepository &jobPostingRepository;

    GreenhouseBoardSource greenhouseBoard;
    LeverBoardSource leverBoard;
    AshbyBoardSource ashbyBoard;
    WorkdayBoardSource workdayBoard;
    SmartRecruitersBoardSource smartRecruitersBoard;

    bool checkRunning = false;
    int checksStarted = 0;
    int checksFinished = 0;
    int jobsStillListed = 0;
    int jobsNewlyGone = 0;
    int jobsStillGone = 0;
    int jobsThatCouldNotBeChecked = 0;
    int jobsWithNoReadableLink = 0;
    QString statusSentence;
};
