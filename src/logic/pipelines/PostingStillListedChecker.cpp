#include "PostingStillListedChecker.h"

#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QStringList>

#include "../../model/JobPostingRepository.h"
#include "../jobscout/AtsBoardDetector.h"
#include "JobPipelines.h"

namespace {

// "1 job" or "3 jobs".
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

QList<AtsBoardIdentity> PostingStillListedChecker::checkableLinksFor(
    const TargetedJob &targetedJob) const
{
    // The job's own link first. Other places Job Crush found the job come
    // after it. Those were found by matching the company and the title, so
    // they are weaker evidence about this exact job.
    QStringList candidateLinks;
    candidateLinks.append(targetedJob.posting.sourceUrl);
    for (const PostingSource &route :
             board.routesToJobPosting(targetedJob.posting.jobPostingId)) {
        candidateLinks.append(route.postingUrl);
    }

    const AtsBoardDetector boardDetector;
    QList<AtsBoardIdentity> checkableLinks;
    QSet<QString> linksAlreadyListed;
    for (const QString &candidateLink : candidateLinks) {
        const AtsBoardIdentity identity = boardDetector.identify(candidateLink);
        if (!identity.namesOneJob() || !goneConfirmer.canCheckBoard(identity.boardName)) {
            continue;
        }
        const QString linkKey = identity.asKey().toLower();
        if (linksAlreadyListed.contains(linkKey)) {
            continue;
        }
        linksAlreadyListed.insert(linkKey);
        checkableLinks.append(identity);
    }
    return checkableLinks;
}

void PostingStillListedChecker::checkEveryOpenCard()
{
    if (checkRunning) {
        return;
    }

    checkRunning = true;
    jobsBeingChecked.clear();
    checksStarted = 0;
    checksFinished = 0;
    jobsStillListed = 0;
    jobsNewlyGone = 0;
    jobsStillGone = 0;
    jobsThatCouldNotBeChecked = 0;
    jobsWithNoReadableLink = 0;
    jobsTheUserSaidAreOpen = 0;

    for (const TargetedJob &targetedJob : board.everyOpenTargetedJob()) {
        // The user checked this one and said it is still open. Their word
        // wins, so Job Crush does not check it.
        if (targetedJob.posting.userSaysStillOpenTimestamp.isValid()) {
            ++jobsTheUserSaidAreOpen;
            continue;
        }

        const QList<AtsBoardIdentity> checkableLinks = checkableLinksFor(targetedJob);
        if (checkableLinks.isEmpty()) {
            ++jobsWithNoReadableLink;
            continue;
        }

        const qint64 jobPostingId = targetedJob.posting.jobPostingId;
        JobBeingChecked &jobBeingChecked = jobsBeingChecked[jobPostingId];
        jobBeingChecked.positionTitle = targetedJob.posting.positionTitle;
        jobBeingChecked.wasAlreadyGone = targetedJob.posting.postingGoneSinceTimestamp.isValid();
        jobBeingChecked.linksStillToAnswer = checkableLinks.count();
        ++checksStarted;

        for (const AtsBoardIdentity &identity : checkableLinks) {
            ListingCheckReply *checkReply = goneConfirmer.checkOneJob(
                identity, targetedJob.posting.positionTitle, this);
            connect(checkReply, &ListingCheckReply::answered, this,
                    [this, jobPostingId, identity](ListingAnswer listingAnswer,
                                                   const QString &whatHappened) {
                // Only the answers that are not "still listed" are logged.
                // Those are the ones a person may need to look into.
                if (listingAnswer != ListingAnswer::StillListed) {
                    qInfo().noquote() << QStringLiteral(
                        "PostingStillListedChecker::checkEveryOpenCard: job posting %1 on "
                        "%2 — %3")
                            .arg(jobPostingId).arg(identity.asKey(), whatHappened);
                }
                recordLinkAnswer(jobPostingId, listingAnswer);
            });
        }
    }

    if (checksStarted == 0) {
        finishTheCheck();
        return;
    }

    statusSentence = QStringLiteral("Checking %1 with their employers…")
                         .arg(jobCountText(checksStarted));
    emit checkStatusChanged();
}

void PostingStillListedChecker::recordLinkAnswer(qint64 jobPostingId,
                                                 ListingAnswer listingAnswer)
{
    // Counts the answer. When it is the last link this job was waiting on,
    // the job is done.
    JobBeingChecked &jobBeingChecked = jobsBeingChecked[jobPostingId];
    switch (listingAnswer) {
    case ListingAnswer::StillListed:   ++jobBeingChecked.linksStillListed;   break;
    case ListingAnswer::Gone:          ++jobBeingChecked.linksGone;          break;
    case ListingAnswer::CouldNotCheck: ++jobBeingChecked.linksCouldNotCheck; break;
    }
    if (--jobBeingChecked.linksStillToAnswer == 0) {
        finishOneJob(jobPostingId);
    }
}

void PostingStillListedChecker::finishOneJob(qint64 jobPostingId)
{
    const JobBeingChecked jobBeingChecked = jobsBeingChecked.value(jobPostingId);

    if (jobBeingChecked.linksStillListed > 0) {
        // Any one link that still lists the job is enough.
        ++jobsStillListed;
        if (jobBeingChecked.wasAlreadyGone) {
            // It is listed again. Employers do repost jobs. An empty time
            // takes the "gone" mark off. A failed save is logged by the
            // repository.
            jobPostingRepository.updatePostingGoneSince(jobPostingId, QDateTime());
        }
    } else if (jobBeingChecked.linksCouldNotCheck > 0) {
        // Some link could not be checked, so Job Crush does not know. The
        // card keeps whatever it said before.
        ++jobsThatCouldNotBeChecked;
        qWarning().noquote() << QStringLiteral(
            "PostingStillListedChecker::finishOneJob: could not check job posting %1 "
            "(\"%2\") — no link gave a sure answer")
                .arg(jobPostingId).arg(jobBeingChecked.positionTitle);
    } else if (jobBeingChecked.wasAlreadyGone) {
        // Keep the first date it went missing.
        ++jobsStillGone;
    } else {
        // Every link said gone, each with two answers that agree.
        ++jobsNewlyGone;
        jobPostingRepository.updatePostingGoneSince(jobPostingId,
                                                    QDateTime::currentDateTime());
    }

    ++checksFinished;
    if (checksFinished < checksStarted) {
        statusSentence = QStringLiteral("Checked %1 of %2…")
                             .arg(checksFinished).arg(jobCountText(checksStarted));
        emit checkStatusChanged();
        return;
    }
    finishTheCheck();
}

void PostingStillListedChecker::finishTheCheck()
{
    checkRunning = false;

    QStringList sentences;
    if (checksStarted == 0 && jobsTheUserSaidAreOpen == 0) {
        sentences.append(jobsWithNoReadableLink == 0
            ? QStringLiteral("There are no open jobs on the board to check.")
            : QStringLiteral("None of these jobs has a link Job Crush can check. It can "
                             "check jobs on Greenhouse, Lever, Ashby, SmartRecruiters, "
                             "Workday and Taleo."));
    } else {
        if (jobsStillListed > 0) {
            sentences.append(QStringLiteral("%1 still listed.")
                                 .arg(jobCountText(jobsStillListed)));
        }
        const int jobsGone = jobsNewlyGone + jobsStillGone;
        if (jobsGone > 0) {
            sentences.append(QStringLiteral("%1 gone from the company's own board. Look "
                                            "for the amber note on the card.")
                                 .arg(jobCountText(jobsGone)));
        }
        if (jobsThatCouldNotBeChecked > 0) {
            sentences.append(QStringLiteral("%1 couldn't be checked for sure. Try again "
                                            "later.")
                                 .arg(jobCountText(jobsThatCouldNotBeChecked)));
        }
        if (jobsTheUserSaidAreOpen > 0) {
            sentences.append(QStringLiteral("%1 you marked as still open %2 not checked.")
                                 .arg(jobCountText(jobsTheUserSaidAreOpen),
                                      jobsTheUserSaidAreOpen == 1 ? QStringLiteral("was")
                                                                  : QStringLiteral("were")));
        }
        if (jobsWithNoReadableLink > 0) {
            sentences.append(QStringLiteral("%1 %2 no link Job Crush can check.")
                                 .arg(jobCountText(jobsWithNoReadableLink),
                                      jobsWithNoReadableLink == 1 ? QStringLiteral("has")
                                                                  : QStringLiteral("have")));
        }
    }
    statusSentence = sentences.join(QLatin1Char(' '));

    // Re-read the board so every card shows what was just found.
    board.loadFromDatabase();
    emit checkStatusChanged();
}
