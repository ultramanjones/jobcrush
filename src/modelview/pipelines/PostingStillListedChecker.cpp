#include "PostingStillListedChecker.h"

#include <QDateTime>
#include <QDebug>
#include <QStringList>

#include "../../model/JobPostingRepository.h"
#include "../jobscout/AtsBoardDetector.h"
#include "../jobscout/JobScoutReply.h"
#include "JobPipelines.h"

namespace {

QString jobCountText(int jobCount)
{
    return jobCount == 1 ? QStringLiteral("1 job") : QStringLiteral("%1 jobs").arg(jobCount);
}

} // namespace

PostingStillListedChecker::PostingStillListedChecker(JobPipelines &pipelines,
                                                     JobPostingRepository &postingRepository,
                                                     QObject *parent)
    : QObject(parent)
    , board(pipelines)
    , jobPostingRepository(postingRepository)
{
}

bool PostingStillListedChecker::checkIsRunning() const
{
    return checkRunning;
}

QString PostingStillListedChecker::checkStatusText() const
{
    return statusSentence;
}

EmployerBoardProvider *PostingStillListedChecker::boardReaderFor(const QString &boardName)
{
    if (boardName == AtsBoardName::Greenhouse) return &greenhouseBoard;
    if (boardName == AtsBoardName::Lever)      return &leverBoard;
    if (boardName == AtsBoardName::Ashby)      return &ashbyBoard;
    if (boardName == AtsBoardName::Workday)    return &workdayBoard;
    return nullptr;
}

AtsBoardIdentity PostingStillListedChecker::readableIdentityFor(
    const TargetedJob &targetedJob) const
{
    // Every route the job is known by, then the posting's own link. The first
    // one that names a single job on a board Job Crush can read wins.
    QStringList candidateLinks;
    for (const PostingSource &route :
             board.routesToJobPosting(targetedJob.posting.jobPostingId)) {
        candidateLinks.append(route.postingUrl);
    }
    candidateLinks.append(targetedJob.posting.sourceUrl);

    const AtsBoardDetector boardDetector;
    for (const QString &candidateLink : candidateLinks) {
        const AtsBoardIdentity identity = boardDetector.identify(candidateLink);
        const bool boardIsReadable = identity.boardName == AtsBoardName::Greenhouse
                                  || identity.boardName == AtsBoardName::Lever
                                  || identity.boardName == AtsBoardName::Ashby
                                  || identity.boardName == AtsBoardName::Workday;
        if (identity.namesOneJob() && boardIsReadable) {
            return identity;
        }
    }
    return AtsBoardIdentity();
}

void PostingStillListedChecker::checkEveryOpenCard()
{
    if (checkRunning) {
        return;
    }

    checkRunning = true;
    checksStarted = 0;
    checksFinished = 0;
    jobsStillListed = 0;
    jobsNewlyGone = 0;
    jobsStillGone = 0;
    jobsThatCouldNotBeChecked = 0;
    jobsWithNoReadableLink = 0;

    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.pipelineStage == PipelineStage::Closed) {
            continue;
        }

        const AtsBoardIdentity identity = readableIdentityFor(targetedJob);
        EmployerBoardProvider *boardReader =
            identity.isKnown() ? boardReaderFor(identity.boardName) : nullptr;
        if (!boardReader) {
            ++jobsWithNoReadableLink;
            continue;
        }

        ++checksStarted;
        const qint64 jobPostingId = targetedJob.posting.jobPostingId;
        const bool wasAlreadyGone = targetedJob.posting.postingGoneSinceTimestamp.isValid();
        const QString positionTitle = targetedJob.posting.positionTitle;

        JobScoutReply *boardReply = boardReader->fetchOneJob(identity, this);
        connect(boardReply, &JobScoutReply::finished, this,
                [this, boardReply, jobPostingId, wasAlreadyGone](
                    const QList<JobPosting> &foundPostings) {
            boardReply->deleteLater();
            recordAnswer(jobPostingId, !foundPostings.isEmpty(), wasAlreadyGone);
            finishOneCheck();
        });
        connect(boardReply, &JobScoutReply::failed, this,
                [this, boardReply, positionTitle](const QString &whyItFailed) {
            boardReply->deleteLater();
            recordTrouble(positionTitle, whyItFailed);
            finishOneCheck();
        });
    }

    if (checksStarted == 0) {
        checkRunning = false;
        statusSentence = jobsWithNoReadableLink == 0
            ? QStringLiteral("There are no open jobs on the board to check.")
            : QStringLiteral("None of these jobs has a link Job Crush can check. It can "
                             "check jobs on Greenhouse, Lever, Ashby and Workday.");
        emit checkStatusChanged();
        return;
    }

    statusSentence = QStringLiteral("Checking %1 with their employers…")
                         .arg(jobCountText(checksStarted));
    emit checkStatusChanged();
}

void PostingStillListedChecker::recordAnswer(qint64 jobPostingId, bool postingIsListed,
                                             bool wasAlreadyGone)
{
    if (postingIsListed) {
        ++jobsStillListed;
        if (wasAlreadyGone) {
            // It came back. Employers do repost, and the flag must not
            // outlive the fact it was about.
            jobPostingRepository.updatePostingGoneSince(jobPostingId, QDateTime());
        }
        return;
    }

    // Keep the first date it went missing. A later check that finds it still
    // gone does not make it newly gone.
    if (wasAlreadyGone) {
        ++jobsStillGone;
        return;
    }
    ++jobsNewlyGone;
    jobPostingRepository.updatePostingGoneSince(jobPostingId, QDateTime::currentDateTime());
}

void PostingStillListedChecker::recordTrouble(const QString &positionTitle,
                                              const QString &whyItFailed)
{
    ++jobsThatCouldNotBeChecked;
    qWarning().noquote() << QStringLiteral(
        "PostingStillListedChecker::recordTrouble: could not check \"%1\" — %2")
        .arg(positionTitle, whyItFailed);
}

void PostingStillListedChecker::finishOneCheck()
{
    ++checksFinished;
    if (checksFinished < checksStarted) {
        statusSentence = QStringLiteral("Checked %1 of %2…")
                             .arg(checksFinished).arg(jobCountText(checksStarted));
        emit checkStatusChanged();
        return;
    }

    checkRunning = false;

    QStringList sentences;
    if (jobsStillListed > 0) {
        sentences.append(QStringLiteral("%1 still listed.").arg(jobCountText(jobsStillListed)));
    }
    const int jobsGone = jobsNewlyGone + jobsStillGone;
    if (jobsGone > 0) {
        sentences.append(QStringLiteral("%1 seem%2 to be gone. Look for the amber note on "
                                        "the card.")
                             .arg(jobCountText(jobsGone),
                                  jobsGone == 1 ? QStringLiteral("s") : QString()));
    }
    if (jobsThatCouldNotBeChecked > 0) {
        sentences.append(QStringLiteral("%1 couldn't be checked. Try again later.")
                             .arg(jobCountText(jobsThatCouldNotBeChecked)));
    }
    if (jobsWithNoReadableLink > 0) {
        sentences.append(QStringLiteral("%1 %2 no link Job Crush can check.")
                             .arg(jobCountText(jobsWithNoReadableLink),
                                  jobsWithNoReadableLink == 1 ? QStringLiteral("has")
                                                              : QStringLiteral("have")));
    }
    statusSentence = sentences.join(QLatin1Char(' '));

    // Re-read the board so every card shows what was just found.
    board.loadFromDatabase();
    emit checkStatusChanged();
}
