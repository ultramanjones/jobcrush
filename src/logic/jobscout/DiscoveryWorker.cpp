#include "DiscoveryWorker.h"

#include <algorithm>

#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QSet>

#include "../../model/JobCrushDatabase.h"
#include "../../model/JobPostingRepository.h"
#include "AtsBoardDetector.h"

namespace {

// The name Qt uses to tell the worker's database connection apart from the
// connection that the screen's thread uses.
const QString workerConnectionName = QStringLiteral("jobCrushDiscoveryWorkerConnection");

// saveFindings saves jobs in groups of jobsSavedPerBatch jobs, with one
// write to the disk per group. While a group is being written, the screen's thread cannot write to
// the database. Small groups give the screen's thread a turn between groups.
// For example, the user may press a job's CRUSH button in the middle of a
// sweep. That click saves to the database from the screen's thread.
constexpr int jobsSavedPerBatch = 200;

} // namespace

QString crossSourceIdentityOf(const JobPosting &jobPosting)
{
    QString identityText = (jobPosting.companyName + jobPosting.positionTitle).toLower();
    identityText.removeIf([](QChar character) {
        return !character.isLetterOrNumber();
    });
    return identityText;
}

DiscoveryWorker::DiscoveryWorker(const QString &databaseFilePath)
    : databaseFilePath(databaseFilePath)
{
}

DiscoveryWorker::~DiscoveryWorker() = default;

void DiscoveryWorker::openDatabase()
{
    workerDatabase = std::make_unique<JobCrushDatabase>();
    databaseIsOpen = workerDatabase->openExtraConnection(databaseFilePath, workerConnectionName);
    if (!databaseIsOpen) {
        qWarning().noquote() << QStringLiteral(
            "DiscoveryWorker::openDatabase: could not open %1 — %2")
                .arg(databaseFilePath, workerDatabase->lastErrorText());
        return;
    }
    workerJobPostingRepository = std::make_unique<JobPostingRepository>(*workerDatabase);
}

void DiscoveryWorker::saveFindings(const QString &sourceStorageName,
                                   const QList<JobPosting> &foundJobPostings)
{
    FindingsSaveOutcome saveOutcome;

    if (!databaseIsOpen) {
        saveOutcome.jobsRefused = foundJobPostings.count();
        saveOutcome.firstRefusalReason =
            QStringLiteral("Job Crush's background database connection is not open");
        emit findingsSaved(sourceStorageName, saveOutcome);
        return;
    }

    for (int batchStart = 0; batchStart < foundJobPostings.count();
         batchStart += jobsSavedPerBatch) {
        const int batchEnd = std::min<int>(batchStart + jobsSavedPerBatch,
                                           foundJobPostings.count());

        // Start one database transaction for this group, so the disk is
        // written once per group, not once per job. If the transaction
        // cannot start, the loop still saves each job, one write per job.
        const bool savingAsOneBatch = workerJobPostingRepository->beginManyChanges();
        for (int jobIndex = batchStart; jobIndex < batchEnd; ++jobIndex) {
            JobPosting foundJobPosting = foundJobPostings.at(jobIndex);
            bool wasAlreadyKnown = false;
            if (!workerJobPostingRepository->insertDiscoveryIfNew(foundJobPosting,
                                                                  wasAlreadyKnown)) {
                // If the database refuses one job, the loop goes on to the
                // next job. But the refusal is counted, and the first error
                // text is kept for the report. If refusals were not counted,
                // a site whose every job was refused would look the same as
                // a site that found no jobs. That mistake once took days to
                // find.
                ++saveOutcome.jobsRefused;
                if (saveOutcome.firstRefusalReason.isEmpty()) {
                    saveOutcome.firstRefusalReason =
                        workerJobPostingRepository->lastErrorText();
                }
                continue;
            }
            ++saveOutcome.jobsSaved;
            if (!wasAlreadyKnown) {
                ++saveOutcome.newJobs;
            }
        }
        if (savingAsOneBatch && !workerJobPostingRepository->finishManyChanges()
                && saveOutcome.firstRefusalReason.isEmpty()) {
            saveOutcome.firstRefusalReason = workerJobPostingRepository->lastErrorText();
        }
    }

    emit findingsSaved(sourceStorageName, saveOutcome);
}

void DiscoveryWorker::findJobsMissingFromBoardsReadInFull(
    const QString &sourceStorageName,
    const QStringList &employersReadInFull,
    const QList<JobPosting> &foundJobPostings)
{
    QList<JobPosting> missingJobPostings;
    if (!databaseIsOpen) {
        qWarning().noquote() << QStringLiteral(
            "DiscoveryWorker::findJobsMissingFromBoardsReadInFull: could not check "
            "\"%1\" for jobs that are gone — the background database connection "
            "is not open").arg(sourceStorageName);
        emit jobsMissingFromBoards(sourceStorageName, missingJobPostings);
        return;
    }
    if (employersReadInFull.isEmpty()) {
        emit jobsMissingFromBoards(sourceStorageName, missingJobPostings);
        return;
    }

    const AtsBoardDetector boardDetector;
    QSet<QString> jobsStillListed;
    for (const JobPosting &foundPosting : foundJobPostings) {
        const AtsBoardIdentity identity = boardDetector.identify(foundPosting.sourceUrl);
        if (identity.namesOneJob()) {
            jobsStillListed.insert(identity.asKey().toLower());
        }
    }

    for (const JobPosting &knownPosting :
         workerJobPostingRepository->loadJobPostingsFromSource(sourceStorageName)) {
        const AtsBoardIdentity identity = boardDetector.identify(knownPosting.sourceUrl);
        if (!identity.namesOneJob()) {
            continue;
        }
        // If this sweep did not read this company's whole board, it cannot
        // tell whether the job is gone, so skip the job.
        const QString employerKey =
            QStringLiteral("%1/%2").arg(identity.boardName, identity.tenant).toLower();
        if (!employersReadInFull.contains(employerKey)) {
            continue;
        }

        const bool isStillListed = jobsStillListed.contains(identity.asKey().toLower());
        const bool wasMarkedGone = knownPosting.postingGoneSinceTimestamp.isValid();
        if (isStillListed && wasMarkedGone) {
            // The job was marked gone, but the board lists it again.
            // Employers do repost jobs. So this clears the gone mark. If the
            // update fails, the repository logs the error.
            workerJobPostingRepository->updatePostingGoneSince(knownPosting.jobPostingId,
                                                               QDateTime());
        } else if (!isStillListed && !wasMarkedGone
                   && !knownPosting.userSaysStillOpenTimestamp.isValid()) {
            missingJobPostings.append(knownPosting);
        }
    }

    emit jobsMissingFromBoards(sourceStorageName, missingJobPostings);
}

void DiscoveryWorker::prepareRows(int requestNumber, const DiscoveryRowRequest &rowRequest,
                                  const ScoringProfile &scoringProfile)
{
    DiscoveryRows preparedRows;
    if (!databaseIsOpen) {
        // Send an empty answer anyway. Otherwise the page would wait forever
        // for its rows.
        qWarning().noquote() << QStringLiteral(
            "DiscoveryWorker::prepareRows: could not load rows for request %1 — the "
            "background database connection is not open").arg(requestNumber);
        emit rowsPrepared(requestNumber, preparedRows);
        return;
    }

    const ProspectScorer prospectScorer(scoringProfile);
    const bool searchAreaIsNarrowed = !scoringProfile.preferredWorkLocations.isEmpty();

    // Three things need every saved job from every site: the Top Prospects
    // tab, the Outside your area tab, and the count of jobs that the location
    // filter hides. If this request needs any of them, load that list once
    // and use it for all three. The count is needed whenever the user has
    // named a place, no matter which tab was asked for.
    QList<JobPosting> allDiscoveredJobPostings;
    const bool needsEveryDiscoveredJob =
        searchAreaIsNarrowed
        || rowRequest.rowKind == DiscoveryRowRequest::RowKind::TopProspects
        || rowRequest.rowKind == DiscoveryRowRequest::RowKind::OutsideSearchArea;
    if (needsEveryDiscoveredJob) {
        allDiscoveredJobPostings = workerJobPostingRepository->loadAllDiscoveredJobPostings();
    }

    if (searchAreaIsNarrowed) {
        for (const JobPosting &discoveredJobPosting : allDiscoveredJobPostings) {
            if (!prospectScorer.jobPostingIsInsideSearchArea(discoveredJobPosting)) {
                ++preparedRows.jobCountOutsideSearchArea;
            }
        }
    }

    switch (rowRequest.rowKind) {
    case DiscoveryRowRequest::RowKind::TopProspects:
        preparedRows.scoredJobPostings =
            rankedJobPostings(allDiscoveredJobPostings, prospectScorer, true);
        break;

    case DiscoveryRowRequest::RowKind::OutsideSearchArea:
        preparedRows.scoredJobPostings =
            rankedJobPostings(allDiscoveredJobPostings, prospectScorer, false);
        break;

    case DiscoveryRowRequest::RowKind::OneSource:
        // A site's own tab keeps the order the database returns: newest
        // posting first. Each row still gets a score, so the row can show
        // it. Only Top Prospects and Outside your area sort by score.
        for (const JobPosting &storedJobPosting :
             workerJobPostingRepository->loadJobPostingsFromSource(
                 rowRequest.sourceStorageName)) {
            if (!prospectScorer.jobPostingIsInsideSearchArea(storedJobPosting)) {
                continue;
            }
            preparedRows.scoredJobPostings.append(
                { storedJobPosting, prospectScorer.scoreJobPosting(storedJobPosting) });
        }
        break;

    case DiscoveryRowRequest::RowKind::HandAdded:
        // The Manual Add tab does not use the location filter. The user
        // added these jobs by hand, so the app never hides them.
        for (const JobPosting &storedJobPosting :
             workerJobPostingRepository->loadHandAddedJobPostings()) {
            preparedRows.scoredJobPostings.append(
                { storedJobPosting, prospectScorer.scoreJobPosting(storedJobPosting) });
        }
        break;
    }

    emit rowsPrepared(requestNumber, preparedRows);
}

QList<ScoredJobPosting> DiscoveryWorker::rankedJobPostings(
    const QList<JobPosting> &allDiscoveredJobPostings,
    const ProspectScorer &prospectScorer,
    bool insideSearchArea) const
{
    // Maps each job's key from crossSourceIdentityOf (company plus title) to
    // the copy of that job with the highest score. So if two sites list the
    // same job, the list shows it once. If two copies have the same score,
    // the first one seen is kept. loadAllDiscoveredJobPostings returns the
    // newest posting first, so that is the newer copy.
    QHash<QString, ScoredJobPosting> bestScoredJobPostingByIdentity;

    for (const JobPosting &discoveredJobPosting : allDiscoveredJobPostings) {
        if (prospectScorer.jobPostingIsInsideSearchArea(discoveredJobPosting)
                != insideSearchArea) {
            continue;
        }
        const ScoredJobPosting scoredJobPosting = {
            discoveredJobPosting, prospectScorer.scoreJobPosting(discoveredJobPosting)
        };
        const QString identity = crossSourceIdentityOf(discoveredJobPosting);

        const auto existingEntry = bestScoredJobPostingByIdentity.constFind(identity);
        if (existingEntry == bestScoredJobPostingByIdentity.constEnd()
                || existingEntry->matchResult.matchScoreOutOfOneHundred
                       < scoredJobPosting.matchResult.matchScoreOutOfOneHundred) {
            bestScoredJobPostingByIdentity.insert(identity, scoredJobPosting);
        }
    }

    QList<ScoredJobPosting> rankedList = bestScoredJobPostingByIdentity.values();

    std::sort(rankedList.begin(), rankedList.end(),
              [](const ScoredJobPosting &leftJobPosting,
                 const ScoredJobPosting &rightJobPosting) {
        if (leftJobPosting.matchResult.matchScoreOutOfOneHundred
                != rightJobPosting.matchResult.matchScoreOutOfOneHundred) {
            return leftJobPosting.matchResult.matchScoreOutOfOneHundred
                   > rightJobPosting.matchResult.matchScoreOutOfOneHundred;
        }
        // If the two scores are equal, the newer posting goes first.
        return leftJobPosting.jobPosting.postedTimestamp
               > rightJobPosting.jobPosting.postedTimestamp;
    });

    return rankedList;
}
