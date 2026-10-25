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

// The name of this thread's database connection.
const QString workerConnectionName = QStringLiteral("jobCrushDiscoveryWorkerConnection");

// How many jobs are saved in one write to the disk. A small batch keeps the
// database free for the screen's thread between batches, for example when
// the user presses CRUSH in the middle of a sweep.
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

        // One write to the disk per batch, not one per job.
        const bool savingAsOneBatch = workerJobPostingRepository->beginManyChanges();
        for (int jobIndex = batchStart; jobIndex < batchEnd; ++jobIndex) {
            JobPosting foundJobPosting = foundJobPostings.at(jobIndex);
            bool wasAlreadyKnown = false;
            if (!workerJobPostingRepository->insertDiscoveryIfNew(foundJobPosting,
                                                                  wasAlreadyKnown)) {
                // One bad row must not stop the others, but it must not be
                // lost quietly either. A site whose every row is refused looks
                // exactly like a site that found nothing. That mistake once
                // took days to find, so refusals are counted and reported.
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
        const QString employerKey =
            QStringLiteral("%1/%2").arg(identity.boardName, identity.tenant).toLower();
        if (!employersReadInFull.contains(employerKey)) {
            continue; // that company did not answer this time, so nothing is known
        }

        const bool isStillListed = jobsStillListed.contains(identity.asKey().toLower());
        const bool wasMarkedGone = knownPosting.postingGoneSinceTimestamp.isValid();
        if (isStillListed && wasMarkedGone) {
            // It is listed again. Employers do repost jobs. A failed update is
            // logged by the repository.
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
        // The page gets an empty answer, so it is not left waiting.
        qWarning().noquote() << QStringLiteral(
            "DiscoveryWorker::prepareRows: could not load rows for request %1 — the "
            "background database connection is not open").arg(requestNumber);
        emit rowsPrepared(requestNumber, preparedRows);
        return;
    }

    const ProspectScorer prospectScorer(scoringProfile);
    const bool searchAreaIsNarrowed = !scoringProfile.preferredWorkLocations.isEmpty();

    // Top Prospects, Outside your area, and the held-back count all start
    // from every found job, so that list is loaded once and shared.
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
        // A site's own tab stays newest first. The score comes along so the
        // row can show it, but only Top Prospects sorts by score.
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
        // No search-area filter. The user asked for these jobs by name, so
        // the filter never hides them.
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
    // The best-scoring copy of each job, keyed by company and title, so a job
    // carried by two sites shows once.
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
        // Equal scores: the newer posting goes first.
        return leftJobPosting.jobPosting.postedTimestamp
               > rightJobPosting.jobPosting.postedTimestamp;
    });

    return rankedList;
}
