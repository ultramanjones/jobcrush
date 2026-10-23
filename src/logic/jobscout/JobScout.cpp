#include "JobScout.h"

#include <algorithm>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>

#include "../../model/JobPostingRepository.h"
#include "../../model/PostingSourceRepository.h"
#include "../aibrain/AiBrain.h"
#include "ArbeitnowJobSource.h"
#include "AtsBoardDetector.h"
#include "BrainJobFinder.h"
#include "CanonicalPostingResolver.h"
#include "FollowedEmployerJobSource.h"
#include "FollowedEmployerRoster.h"
#include "JobLead.h"
#include "JobScoutReply.h"
#include "JobSearchProfile.h"
#include "JobSourceProvider.h"
#include "JobSourceRoster.h"
#include "JobicyJobSource.h"
#include "PastedTextJobReader.h"
#include "PostedJobPageReader.h"
#include "RemotiveJobSource.h"
#include "UsaJobsJobSource.h"

namespace {

// Two boards can carry the same job. This key is how Top Prospects tells
// that two postings are the same job: the company and the title together,
// lowercase, with everything except letters and numbers taken out. That way
// a difference in spacing or punctuation between two boards does not make
// the same job show twice.
QString crossSourceIdentityOf(const JobPosting &jobPosting)
{
    QString identityText =
        (jobPosting.companyName + jobPosting.positionTitle).toLower();
    identityText.removeIf([](QChar character) {
        return !character.isLetterOrNumber();
    });
    return identityText;
}

} // namespace

JobScout::JobScout(JobPostingRepository &jobPostingRepository,
                   PostingSourceRepository &postingSourceRepository,
                   JobSourceRoster &jobSourceRoster,
                   FollowedEmployerRoster &followedEmployerRoster,
                   JobSearchProfile &searchProfile,
                   AiBrain &aiBrain,
                   const QString &diagnosticsFolderPath,
                   QObject *parent)
    : QObject(parent)
    , sweepLogFolderPath(diagnosticsFolderPath)
    , jobPostingRepository(jobPostingRepository)
    , postingSourceRepository(postingSourceRepository)
    , jobSourceRoster(jobSourceRoster)
    , followedEmployerRoster(followedEmployerRoster)
    , searchProfile(searchProfile)
{
    // One client per job site, built once and kept. Adding a site means
    // writing its class and adding one line here. Nothing above JobScout
    // changes.
    builtJobSourceProviders.push_back(
        std::make_unique<FollowedEmployerJobSource>(followedEmployerRoster));
    builtJobSourceProviders.push_back(std::make_unique<RemotiveJobSource>());
    builtJobSourceProviders.push_back(std::make_unique<ArbeitnowJobSource>());
    builtJobSourceProviders.push_back(std::make_unique<JobicyJobSource>());
    builtJobSourceProviders.push_back(std::make_unique<UsaJobsJobSource>(jobSourceRoster));

    employerBoardResolver = std::make_unique<CanonicalPostingResolver>(this);
    postedPageReader = std::make_unique<PostedJobPageReader>(this);
    brainJobFinder = std::make_unique<BrainJobFinder>(aiBrain, this);
    pastedTextReader = std::make_unique<PastedTextJobReader>(aiBrain);

    // Editing the profile ranks every stored job again. This is instant and
    // free, because the scorer is arithmetic, not a paid AI call.
    connect(&searchProfile, &JobSearchProfile::searchProfileChanged,
            this, &JobScout::discoveriesChanged);

    // Ticking a site changes which tabs exist and what a sweep covers.
    connect(&jobSourceRoster, &JobSourceRoster::enabledSourcesChanged,
            this, &JobScout::discoveriesChanged);

    connect(&followedEmployerRoster, &FollowedEmployerRoster::followedEmployersChanged,
            this, &JobScout::discoveriesChanged);
}

JobScout::~JobScout() = default;

JobSourceProvider *JobScout::providerFor(const QString &sourceStorageName) const
{
    for (const std::unique_ptr<JobSourceProvider> &builtProvider : builtJobSourceProviders) {
        if (builtProvider->descriptor().storageName == sourceStorageName) {
            return builtProvider.get();
        }
    }
    return nullptr; // no client has been written for this site yet
}

bool JobScout::sweepIsRunning() const
{
    return !sourcesStillSweeping.isEmpty();
}

QString JobScout::sweepProgressText() const
{
    QStringList progressPieces = finishedSourceOutcomeLines;

    for (const QString &sourceStorageName : sourcesStillSweeping) {
        bool descriptorFound = false;
        const JobSourceDescriptor descriptor =
            jobSourceDescriptorFor(sourceStorageName, descriptorFound);
        progressPieces.append(QStringLiteral("%1: looking…")
                                  .arg(descriptorFound ? descriptor.displayName
                                                       : sourceStorageName));
    }
    return progressPieces.join(QStringLiteral("   ·   "));
}

QString JobScout::lastSweepSummaryText() const
{
    return storedLastSweepSummaryText;
}

QString JobScout::lastSweepNoticeText() const
{
    return storedLastSweepNoticeText;
}

QString JobScout::lastSweepTroubleText() const
{
    return storedLastSweepTroubleText;
}

void JobScout::startSweep()
{
    if (sweepIsRunning()) {
        return; // one sweep at a time
    }

    const QStringList enabledSourceNames = jobSourceRoster.enabledSourceStorageNames();
    if (enabledSourceNames.isEmpty()) {
        storedLastSweepSummaryText =
            QStringLiteral("No job sites are ticked yet — pick some in Settings.");
        emit sweepProgressChanged();
        return;
    }

    finishedSourceOutcomeLines.clear();
    sourcesThatHadTroubleThisSweep.clear();
    sourceNoticesThisSweep.clear();
    totalJobsFoundThisSweep = 0;
    totalNewJobsThisSweep = 0;
    storedLastSweepSummaryText.clear();
    storedLastSweepTroubleText.clear();

    // Fill the list of running sites BEFORE starting any of them. A site that
    // answers from its cache can finish at once. If the list were still
    // empty then, the sweep would report itself done before it started.
    for (const QString &sourceStorageName : enabledSourceNames) {
        if (providerFor(sourceStorageName) != nullptr) {
            sourcesStillSweeping.append(sourceStorageName);
        }
    }

    if (sourcesStillSweeping.isEmpty()) {
        storedLastSweepSummaryText =
            QStringLiteral("None of the ticked sites are wired up yet.");
        emit sweepProgressChanged();
        return;
    }

    emit sweepProgressChanged();

    const QStringList sourcesToSweep = sourcesStillSweeping;
    for (const QString &sourceStorageName : sourcesToSweep) {
        beginSweepOfSource(sourceStorageName);
    }
}

void JobScout::beginSweepOfSource(const QString &sourceStorageName)
{
    JobSourceProvider *sourceProvider = providerFor(sourceStorageName);
    if (sourceProvider == nullptr) {
        return;
    }

    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    const QString sourceDisplayName =
        descriptorFound ? descriptor.displayName : sourceStorageName;

    JobScoutReply *scoutReply = sourceProvider->searchForJobs(searchProfile, this);

    connect(scoutReply, &JobScoutReply::finished, this,
            [this, scoutReply, sourceStorageName, sourceDisplayName](
                const QList<JobPosting> &foundJobPostings) {
        const int newJobCountBefore = totalNewJobsThisSweep;
        recordFindingsFromSource(foundJobPostings);
        markJobsGoneFromBoardsReadInFull(sourceStorageName,
                                         scoutReply->employersReadInFull(),
                                         foundJobPostings);
        const int newJobsFromThisSource = totalNewJobsThisSweep - newJobCountBefore;

        // A site that answers with no jobs is not an error, but it is not a
        // success either. The outcome line says so, so the user knows why
        // that site's tab is empty.
        const bool sourceCameBackEmpty = foundJobPostings.isEmpty();

        // Jobs arrived but Job Crush could not save them. That is a problem
        // in Job Crush, not with the site, and the outcome line says so.
        const bool everyJobWasRefused = !foundJobPostings.isEmpty()
            && jobPostingsRefusedThisSource == foundJobPostings.count();

        QString sourceOutcomeText;
        if (sourceCameBackEmpty) {
            sourceOutcomeText = QStringLiteral("%1: answered, but sent no jobs back")
                                    .arg(sourceDisplayName);
        } else if (everyJobWasRefused) {
            sourceOutcomeText =
                QStringLiteral("%1: %2 jobs arrived but Job Crush could not store any "
                               "of them — %3")
                    .arg(sourceDisplayName)
                    .arg(foundJobPostings.count())
                    .arg(firstRefusalReasonThisSource);
        } else if (jobPostingsRefusedThisSource > 0) {
            sourceOutcomeText =
                QStringLiteral("%1: %2 found, %3 new, %4 could not be stored — %5")
                    .arg(sourceDisplayName)
                    .arg(foundJobPostings.count())
                    .arg(newJobsFromThisSource)
                    .arg(jobPostingsRefusedThisSource)
                    .arg(firstRefusalReasonThisSource);
        } else {
            sourceOutcomeText = QStringLiteral("%1: %2 found, %3 new")
                                    .arg(sourceDisplayName)
                                    .arg(foundJobPostings.count())
                                    .arg(newJobsFromThisSource);
        }

        finishSourceAndReportProgress(
            sourceStorageName,
            sourceOutcomeText,
            sourceCameBackEmpty || jobPostingsRefusedThisSource > 0);

        scoutReply->deleteLater();
    });

    connect(scoutReply, &JobScoutReply::failed, this,
            [this, scoutReply, sourceStorageName, sourceDisplayName](
                const QString &humanReadableReason, bool sourceHadTrouble) {
        // One site failing does not stop the sweep. Record what it said and
        // let the other sites finish.
        //
        // Not every failure is trouble. A site that asked Job Crush to wait an
        // hour, while Job Crush waits, is working as it should. That is shown
        // as a notice, not as an error.
        if (!sourceHadTrouble) {
            sourceNoticesThisSweep.append(
                QStringLiteral("%1: %2").arg(sourceDisplayName, humanReadableReason));
        }
        finishSourceAndReportProgress(
            sourceStorageName,
            QStringLiteral("%1: %2").arg(sourceDisplayName, humanReadableReason),
            sourceHadTrouble);

        scoutReply->deleteLater();
    });
}

void JobScout::markJobsGoneFromBoardsReadInFull(const QString &sourceStorageName,
                                                const QStringList &employersReadInFull,
                                                const QList<JobPosting> &foundJobPostings)
{
    if (employersReadInFull.isEmpty()) {
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

    const QDateTime checkedTimestamp = QDateTime::currentDateTime();
    const bool savingAsOneBatch = jobPostingRepository.beginManyChanges();
    for (const JobPosting &knownPosting :
         jobPostingRepository.loadJobPostingsFromSource(sourceStorageName)) {
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
            // It is listed again. Employers do repost jobs.
            jobPostingRepository.updatePostingGoneSince(knownPosting.jobPostingId,
                                                        QDateTime());
        } else if (!isStillListed && !wasMarkedGone) {
            jobPostingRepository.updatePostingGoneSince(knownPosting.jobPostingId,
                                                        checkedTimestamp);
        }
        // A failed update is logged by the repository. The mark is shown to
        // the user as a hint, so one failed row does not stop the others.
    }
    if (savingAsOneBatch) {
        jobPostingRepository.finishManyChanges(); // logs its own failure
    }
}

void JobScout::recordFindingsFromSource(const QList<JobPosting> &foundJobPostings)
{
    jobPostingsRefusedThisSource = 0;
    firstRefusalReasonThisSource.clear();

    // One batch, so the disk is written once and not once per job.
    const bool savingAsOneBatch = jobPostingRepository.beginManyChanges();
    for (JobPosting foundJobPosting : foundJobPostings) {
        bool wasAlreadyKnown = false;
        if (!jobPostingRepository.insertDiscoveryIfNew(foundJobPosting,
                                                       wasAlreadyKnown)) {
            // One bad row must not stop the sweep, but it must not be lost
            // quietly either. A site whose every row is refused looks exactly
            // like a site that found nothing. That mistake once took days to
            // find, so the refusals are counted and shown.
            ++jobPostingsRefusedThisSource;
            if (firstRefusalReasonThisSource.isEmpty()) {
                firstRefusalReasonThisSource = jobPostingRepository.lastErrorText();
            }
            continue;
        }
        ++totalJobsFoundThisSweep;
        if (!wasAlreadyKnown) {
            ++totalNewJobsThisSweep;
        }
    }
    if (savingAsOneBatch && !jobPostingRepository.finishManyChanges()) {
        if (firstRefusalReasonThisSource.isEmpty()) {
            firstRefusalReasonThisSource = jobPostingRepository.lastErrorText();
        }
    }
}

void JobScout::finishSourceAndReportProgress(const QString &sourceStorageName,
                                             const QString &sourceOutcomeText,
                                             bool sourceHadTrouble)
{
    sourcesStillSweeping.removeAll(sourceStorageName);
    finishedSourceOutcomeLines.append(sourceOutcomeText);
    if (sourceHadTrouble) {
        sourcesThatHadTroubleThisSweep.append(sourceOutcomeText);
    }

    if (sourcesStillSweeping.isEmpty()) {
        const int sitesSwept = finishedSourceOutcomeLines.count();
        storedLastSweepSummaryText =
            QStringLiteral("%1 jobs checked · %2 new · %3 %4")
                .arg(totalJobsFoundThisSweep)
                .arg(totalNewJobsThisSweep)
                .arg(sitesSwept)
                .arg(sitesSwept == 1 ? QStringLiteral("site") : QStringLiteral("sites"));

        // Whatever went wrong is kept after the sweep ends. The summary does
        // not replace it.
        storedLastSweepTroubleText =
            sourcesThatHadTroubleThisSweep.join(QStringLiteral("   ·   "));
        storedLastSweepNoticeText =
            sourceNoticesThisSweep.join(QStringLiteral("   ·   "));

        writeSweepLog();

        emit discoveriesChanged();
    }

    emit sweepProgressChanged();
}

QList<ScoredJobPosting> JobScout::scoredJobPostingsFromSource(
    const QString &sourceStorageName, SearchAreaScope searchAreaScope) const
{
    const ProspectScorer prospectScorer(searchProfile);

    QList<ScoredJobPosting> scoredJobPostings;
    const QList<JobPosting> storedJobPostings =
        jobPostingRepository.loadJobPostingsFromSource(sourceStorageName);

    scoredJobPostings.reserve(storedJobPostings.count());
    for (const JobPosting &storedJobPosting : storedJobPostings) {
        const bool jobIsInsideSearchArea =
            prospectScorer.jobPostingIsInsideSearchArea(storedJobPosting);
        if (jobIsInsideSearchArea != (searchAreaScope == SearchAreaScope::InsideSearchArea)) {
            continue;
        }
        // A site's own tab stays newest first. The score comes along so the
        // row can show it, but only Top Prospects sorts by score.
        scoredJobPostings.append(
            { storedJobPosting, prospectScorer.scoreJobPosting(storedJobPosting) });
    }
    return scoredJobPostings;
}

QList<ScoredJobPosting> JobScout::handAddedJobPostings() const
{
    const ProspectScorer prospectScorer(searchProfile);

    QList<ScoredJobPosting> scoredJobPostings;
    const QList<JobPosting> storedJobPostings =
        jobPostingRepository.loadHandAddedJobPostings();

    scoredJobPostings.reserve(storedJobPostings.count());
    for (const JobPosting &storedJobPosting : storedJobPostings) {
        // No search-area test here. See the comment on the declaration.
        scoredJobPostings.append(
            { storedJobPosting, prospectScorer.scoreJobPosting(storedJobPosting) });
    }
    return scoredJobPostings;
}

qint64 JobScout::mostRecentlyHandAddedJobPostingId() const
{
    return lastHandAddedJobPostingId;
}

int JobScout::jobPostingCountOutsideSearchArea() const
{
    if (!searchAreaIsNarrowed()) {
        return 0; // nothing is being filtered, so nothing is being held back
    }

    const ProspectScorer prospectScorer(searchProfile);
    int heldBackCount = 0;
    for (const JobPosting &discoveredJobPosting :
             jobPostingRepository.loadAllDiscoveredJobPostings()) {
        if (!prospectScorer.jobPostingIsInsideSearchArea(discoveredJobPosting)) {
            ++heldBackCount;
        }
    }
    return heldBackCount;
}

bool JobScout::searchAreaIsNarrowed() const
{
    return !searchProfile.preferredWorkLocations().isEmpty();
}

bool JobScout::searchProfileCanRank() const
{
    return searchProfile.hasEnoughToRankBy();
}

QList<ScoredJobPosting> JobScout::rankedTopProspects(SearchAreaScope searchAreaScope) const
{
    const ProspectScorer prospectScorer(searchProfile);

    // The best-scoring copy of each job, keyed by company and title, so a job
    // carried by two sites shows once.
    QHash<QString, ScoredJobPosting> bestScoredJobPostingByIdentity;

    const QList<JobPosting> allDiscoveredJobPostings =
        jobPostingRepository.loadAllDiscoveredJobPostings();

    for (const JobPosting &discoveredJobPosting : allDiscoveredJobPostings) {
        const bool jobIsInsideSearchArea =
            prospectScorer.jobPostingIsInsideSearchArea(discoveredJobPosting);
        if (jobIsInsideSearchArea != (searchAreaScope == SearchAreaScope::InsideSearchArea)) {
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

    QList<ScoredJobPosting> rankedJobPostings = bestScoredJobPostingByIdentity.values();

    std::sort(rankedJobPostings.begin(), rankedJobPostings.end(),
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

    return rankedJobPostings;
}

void JobScout::writeSweepLog() const
{
    if (sweepLogFolderPath.isEmpty()) {
        return;
    }
    QDir().mkpath(sweepLogFolderPath);

    QFile sweepLogFile(QDir(sweepLogFolderPath).filePath(QStringLiteral("lastSweep.log")));
    if (!sweepLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        // The sweep itself worked, so it is not undone. The failure is logged.
        qWarning().noquote() << QStringLiteral(
            "JobScout::writeSweepLog: could not open %1 for writing — %2")
                .arg(sweepLogFile.fileName(), sweepLogFile.errorString());
        return;
    }

    QTextStream sweepLogStream(&sweepLogFile);
    sweepLogStream << "Job Crush — JobScout sweep log\n";
    sweepLogStream << "Finished: "
                   << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    sweepLogStream << "Summary:  " << storedLastSweepSummaryText << "\n";
    sweepLogStream << "\nWhat each site said:\n";
    for (const QString &sourceOutcomeLine : finishedSourceOutcomeLines) {
        sweepLogStream << "  - " << sourceOutcomeLine << "\n";
    }
}


// ---------------------------------------------------------------------------
// Adding one job by hand
// ---------------------------------------------------------------------------

void JobScout::readPastedText(const QString &pastedText)
{
    if (pastedTextBeingRead) {
        return;
    }
    pastedTextBeingRead = true;
    storedPastedTextStatus = QStringLiteral("Moonlight is reading it…");
    storedFilledForm = JobPosting();
    emit pastedTextFormChanged();

    JobScoutReply *readerReply = pastedTextReader->readJob(pastedText, this);
    const QString keptText = pastedText.trimmed();
    connect(readerReply, &JobScoutReply::finished, this,
            [this, readerReply, keptText](const QList<JobPosting> &postingsRead) {
        readerReply->deleteLater();
        pastedTextBeingRead = false;
        storedFilledForm = postingsRead.isEmpty() ? JobPosting() : postingsRead.first();
        // When Moonlight found no description, the whole pasted text is used
        // as the description, so nothing the user gave is lost.
        if (storedFilledForm.fullDescriptionText.isEmpty()) {
            storedFilledForm.fullDescriptionText = keptText;
        }
        storedPastedTextStatus = QStringLiteral(
            "Moonlight filled in what the text says. Check it, fill in anything "
            "marked empty if you know it, then press Add this job.");
        emit pastedTextFormChanged();
    });
    connect(readerReply, &JobScoutReply::failed, this,
            [this, readerReply](const QString &whyItFailed) {
        readerReply->deleteLater();
        pastedTextBeingRead = false;
        storedPastedTextStatus = whyItFailed;
        emit pastedTextFormChanged();
    });
}

bool JobScout::pastedTextIsBeingRead() const
{
    return pastedTextBeingRead;
}

QString JobScout::pastedTextStatusText() const
{
    return storedPastedTextStatus;
}

bool JobScout::aBrainCanReadPastedText() const
{
    // Reading text needs no web search, so any connected brain will do,
    // Ollama included.
    return brainJobFinder->aBrainIsAvailable();
}

JobPosting JobScout::filledFormFromPastedText() const
{
    return storedFilledForm;
}

void JobScout::clearFilledForm()
{
    storedFilledForm = JobPosting();
    storedPastedTextStatus.clear();
    emit pastedTextFormChanged();
}

void JobScout::addJobFromFilledForm(const JobPosting &formFields)
{
    if (formFields.companyName.trimmed().isEmpty()
            || formFields.positionTitle.trimmed().isEmpty()) {
        storedPastedTextStatus = QStringLiteral(
            "Job Crush needs at least the company and the job title. Fill in both "
            "and press Add this job again.");
        emit pastedTextFormChanged();
        return;
    }
    if (aLeadIsBeingResolved) {
        sayThisAboutTheLead(QStringLiteral(
            "Job Crush is still looking for the last one. Give it a moment."));
        return;
    }

    JobLead jobLead;
    jobLead.companyName = formFields.companyName.trimmed();
    jobLead.positionTitle = formFields.positionTitle.trimmed();
    jobLead.locationText = formFields.locationText.trimmed();
    jobLead.salaryText = formFields.salaryText.trimmed();
    jobLead.isRemoteRole = formFields.isRemoteRole;
    jobLead.postedTimestamp = formFields.postedTimestamp;
    jobLead.rawText = formFields.fullDescriptionText;
    jobLead.discoveryUrl = formFields.sourceUrl.trimmed();
    if (!jobLead.discoveryUrl.isEmpty()) {
        jobLead.boardIdentity = AtsBoardDetector().identify(jobLead.discoveryUrl);
        if (jobLead.boardIdentity.isKnown()) {
            jobLead.postingSource = jobLead.boardIdentity.boardName;
        }
    }

    storedFilledForm = JobPosting();
    storedPastedTextStatus.clear();
    emit pastedTextFormChanged();

    // The same hunt as every other added job: the employer's own board, then
    // the page, then the brain. If none of them has it, the form is saved as
    // the user left it.
    beginHuntForOneLead(jobLead);
}

void JobScout::lookForOtherPlacesToApply(qint64 jobPostingId)
{
    bool jobPostingWasFound = false;
    const JobPosting jobPosting =
        jobPostingRepository.loadJobPostingById(jobPostingId, jobPostingWasFound);
    if (!jobPostingWasFound) {
        qWarning().noquote() << QStringLiteral(
            "JobScout::lookForOtherPlacesToApply: no job posting has id %1")
                .arg(jobPostingId);
        return;
    }
    if (jobPosting.companyName.trimmed().isEmpty()
            || jobPosting.positionTitle.trimmed().isEmpty()) {
        return; // nothing to search with
    }

    // The place the job was found is a place to apply too. It is recorded
    // first, so the job always has at least that one.
    if (!jobPosting.sourceUrl.isEmpty() && !jobPosting.postingSource.isEmpty()) {
        rememberRouteToThisJob(jobPostingId, jobPosting.postingSource,
                               jobPosting.externalSourceId, jobPosting.sourceUrl);
    }

    // The job was already read off the employer's own board. There is
    // nothing better to find, so no requests are sent.
    if (AtsBoardDetector().identify(jobPosting.sourceUrl).namesOneJob()) {
        return;
    }

    JobLead jobLead;
    jobLead.companyName = jobPosting.companyName;
    jobLead.positionTitle = jobPosting.positionTitle;
    jobLead.locationText = jobPosting.locationText;

    JobScoutReply *resolverReply = employerBoardResolver->resolve(jobLead, this);
    connect(resolverReply, &JobScoutReply::finished, this,
            [this, resolverReply, jobPostingId, jobPosting](
                const QList<JobPosting> &foundPostings) {
        resolverReply->deleteLater();
        if (foundPostings.isEmpty() || foundPostings.first().sourceUrl.isEmpty()
                || foundPostings.first().sourceUrl == jobPosting.sourceUrl) {
            return;
        }
        const JobPosting &employersOwnPosting = foundPostings.first();
        rememberRouteToThisJob(jobPostingId, employersOwnPosting.postingSource,
                               employersOwnPosting.externalSourceId,
                               employersOwnPosting.sourceUrl);
        emit placesToApplyChanged(jobPostingId);
    });
    // Finding nothing is a normal answer here, so the user is not told. The
    // reason is still logged, in case a board has stopped answering.
    connect(resolverReply, &JobScoutReply::failed, this,
            [resolverReply, jobPostingId](const QString &whyItFailed) {
        resolverReply->deleteLater();
        qInfo().noquote() << QStringLiteral(
            "JobScout::lookForOtherPlacesToApply: no other place found for job "
            "posting %1 — %2").arg(jobPostingId).arg(whyItFailed);
    });
}

bool JobScout::leadIsBeingResolved() const
{
    return aLeadIsBeingResolved;
}

QString JobScout::leadStatusText() const
{
    return storedLeadStatusText;
}

void JobScout::addJobFromLink(const QString &pastedLink)
{
    const QString trimmedLink = pastedLink.trimmed();
    if (trimmedLink.isEmpty()) {
        finishLeadWith(QStringLiteral(
            "Paste a link to the job, or type the company name and the job title."));
        return;
    }
    if (aLeadIsBeingResolved) {
        // sayThisAboutTheLead, not finishLeadWith. finishLeadWith would turn
        // the buttons back on and let a second search start beside the first.
        sayThisAboutTheLead(QStringLiteral(
            "Job Crush is still looking for the last one. Give it a moment."));
        return;
    }

    JobLead jobLead;
    jobLead.discoveryUrl = trimmedLink;

    AtsBoardDetector boardDetector;
    jobLead.boardIdentity = boardDetector.identify(trimmedLink);

    // Only a real board name goes in here. A link from a site Job Crush is not
    // allowed to read names no board, so the field stays empty. That the
    // user pasted it is recorded separately, as the posting's scout source.
    jobLead.postingSource = jobLead.boardIdentity.isKnown()
        ? jobLead.boardIdentity.boardName
        : QString();

    // Job Crush does not open LinkedIn or Indeed links, and the link itself
    // does not say which job it is. Tell the user now, and say what to do
    // instead.
    if (!jobLead.boardIdentity.isKnown() && boardDetector.isWalledGarden(trimmedLink)) {
        finishLeadWith(QStringLiteral(
            "Job Crush isn't allowed to read that site, and the link alone doesn't "
            "say which job it is. Type the company name and the job title off the "
            "page instead and Job Crush will go find the employer's own posting."));
        return;
    }

    beginHuntForOneLead(jobLead);
}

void JobScout::addJobFromCompanyAndTitle(const QString &companyName,
                                         const QString &positionTitle)
{
    const QString trimmedCompany = companyName.trimmed();
    const QString trimmedTitle = positionTitle.trimmed();

    if (trimmedCompany.isEmpty() || trimmedTitle.isEmpty()) {
        finishLeadWith(QStringLiteral(
            "Job Crush needs both the company name and the job title to go looking. "
            "Fill in both and try again."));
        return;
    }
    if (aLeadIsBeingResolved) {
        // sayThisAboutTheLead, not finishLeadWith. finishLeadWith would turn
        // the buttons back on and let a second search start beside the first.
        sayThisAboutTheLead(QStringLiteral(
            "Job Crush is still looking for the last one. Give it a moment."));
        return;
    }

    JobLead jobLead;
    jobLead.companyName = trimmedCompany;
    jobLead.positionTitle = trimmedTitle;
    // No link, so no board is known yet. A resolver may fill this in.

    beginHuntForOneLead(jobLead);
}

// The hunt for one hand-added job, in the order the steps are tried:
//
//   1. The employer's own board, when the link names one or the company
//      name leads to one. This is the best result: the employer's own
//      words, and a posting that goes away when the job is filled.
//   2. The page the user pasted, read off the web. Most job pages hold the
//      job in a labeled block, and the reader knows how to find it.
//   3. The AI brain, asked to search the web. Only after steps 1 and 2 find
//      nothing, and only when a brain is connected, because it costs money.
//      The brain finds a link, and Job Crush goes back to step 1 or 2 to
//      read it. That way the saved posting is the employer's text, not the
//      brain's summary of it.
//
// When every step misses, what the user gave is saved anyway.
void JobScout::beginHuntForOneLead(const JobLead &jobLead)
{
    aLeadIsBeingResolved = true;
    // Forget the last added job now, so the "show me" button cannot jump to
    // the wrong row while this one is being found.
    lastHandAddedJobPostingId = 0;

    if (jobLead.boardIdentity.isKnown()
            || (!jobLead.companyName.trimmed().isEmpty()
                && !jobLead.positionTitle.trimmed().isEmpty())) {
        huntTheEmployerBoards(jobLead, false);
        return;
    }
    readThePostedPage(jobLead, false);
}

void JobScout::huntTheEmployerBoards(const JobLead &jobLead, bool brainWasAlreadyAsked)
{
    sayThisAboutTheLead(jobLead.companyName.isEmpty()
        ? QStringLiteral("Looking for that job on the employer's own board…")
        : QStringLiteral("Looking for that job on %1's own board…").arg(jobLead.companyName));

    JobScoutReply *resolverReply = employerBoardResolver->resolve(jobLead, this);

    connect(resolverReply, &JobScoutReply::finished, this,
            [this, resolverReply, jobLead, brainWasAlreadyAsked](
                const QList<JobPosting> &foundPostings) {
        resolverReply->deleteLater();
        if (!foundPostings.isEmpty()) {
            keepTheRealPosting(jobLead, foundPostings.first(), brainWasAlreadyAsked, false);
            return;
        }
        whenTheHuntCameUpEmpty(jobLead, QStringLiteral(
            "Job Crush didn't find %1 on Greenhouse, Lever, Ashby or SmartRecruiters, "
            "or on a Workday site it knows.").arg(
                jobLead.companyName.isEmpty() ? QStringLiteral("that company")
                                              : jobLead.companyName),
            brainWasAlreadyAsked);
    });

    connect(resolverReply, &JobScoutReply::failed, this,
            [this, resolverReply, jobLead, brainWasAlreadyAsked](const QString &whyItFailed) {
        resolverReply->deleteLater();
        whenTheHuntCameUpEmpty(jobLead, whyItFailed, brainWasAlreadyAsked);
    });
}

void JobScout::readThePostedPage(const JobLead &jobLead, bool brainWasAlreadyAsked)
{
    sayThisAboutTheLead(QStringLiteral("Reading that page…"));

    JobScoutReply *pageReply = postedPageReader->readJobFromPage(jobLead.discoveryUrl, this);

    connect(pageReply, &JobScoutReply::finished, this,
            [this, pageReply, jobLead, brainWasAlreadyAsked](
                const QList<JobPosting> &foundPostings) {
        pageReply->deleteLater();
        if (!foundPostings.isEmpty()) {
            keepTheRealPosting(jobLead, foundPostings.first(), brainWasAlreadyAsked,
                               postedPageReader->lastReadUsedThePageTitleOnly());
            return;
        }
        whenTheHuntCameUpEmpty(jobLead, QStringLiteral(
            "That page had no job Job Crush could read — some sites only show the "
            "job after a browser runs their scripts."), brainWasAlreadyAsked);
    });

    connect(pageReply, &JobScoutReply::failed, this,
            [this, pageReply, jobLead, brainWasAlreadyAsked](const QString &whyItFailed) {
        pageReply->deleteLater();
        whenTheHuntCameUpEmpty(jobLead, whyItFailed, brainWasAlreadyAsked);
    });
}

void JobScout::whenTheHuntCameUpEmpty(const JobLead &jobLead,
                                      const QString &whyNothingWasFound,
                                      bool brainWasAlreadyAsked)
{
    if (!brainWasAlreadyAsked && brainJobFinder->aBrainCanSearchTheWeb()) {
        askTheBrainToFindIt(jobLead, whyNothingWasFound);
        return;
    }

    QString whatToSay = whyNothingWasFound;
    if (brainWasAlreadyAsked) {
        // The brain gave a link, and Job Crush could not read the page. The
        // job is still saved, and the user can open the link to check it.
        whatToSay = QStringLiteral("%1 found a link but Job Crush couldn't read the "
                                   "page: %2 Open the link to check it.")
            .arg(brainJobFinder->brainDisplayName(), whyNothingWasFound);
    } else if (!brainJobFinder->aBrainIsAvailable()) {
        whatToSay += QStringLiteral(" Connect an AI brain in Settings and Job Crush "
                                    "will search the web for it next time.");
    } else if (!brainJobFinder->aBrainCanSearchTheWeb()) {
        whatToSay += QStringLiteral(" %1 runs on your computer and can't search the "
                                    "web, so it wasn't asked.")
                         .arg(brainJobFinder->brainDisplayName());
    }
    finishLeadWith(keepWhatTheUserGaveUs(jobLead, whatToSay));
}

void JobScout::askTheBrainToFindIt(const JobLead &jobLead,
                                   const QString &whyEarlierStepsMissed)
{
    const QString brainName = brainJobFinder->brainDisplayName();
    sayThisAboutTheLead(QStringLiteral("%1 Asking %2 to search the web for it…")
                            .arg(whyEarlierStepsMissed, brainName));

    JobScoutReply *brainReply = brainJobFinder->findTheJob(jobLead, this);

    connect(brainReply, &JobScoutReply::finished, this,
            [this, brainReply, jobLead](const QList<JobPosting> &foundLeads) {
        brainReply->deleteLater();
        if (foundLeads.isEmpty()) {
            whenTheHuntCameUpEmpty(jobLead, QStringLiteral("nothing came back."), true);
            return;
        }
        const JobPosting &brainsAnswer = foundLeads.first();

        // The brain's answer is a lead, not a posting. Job Crush reads the
        // page it points to, the same way it reads a pasted link, so what
        // gets saved is the employer's text. Where the brain left a field
        // blank, what the user typed is kept.
        JobLead foundLead;
        foundLead.discoveryUrl = brainsAnswer.sourceUrl;
        foundLead.positionTitle = brainsAnswer.positionTitle.isEmpty()
            ? jobLead.positionTitle : brainsAnswer.positionTitle;
        foundLead.companyName = brainsAnswer.companyName.isEmpty()
            ? jobLead.companyName : brainsAnswer.companyName;
        foundLead.locationText = brainsAnswer.locationText;
        foundLead.isRemoteRole = brainsAnswer.isRemoteRole;
        foundLead.rawText = brainsAnswer.fullDescriptionText;

        AtsBoardDetector boardDetector;
        foundLead.boardIdentity = boardDetector.identify(foundLead.discoveryUrl);
        foundLead.postingSource = foundLead.boardIdentity.isKnown()
            ? foundLead.boardIdentity.boardName : QString();

        if (foundLead.boardIdentity.namesOneJob()) {
            huntTheEmployerBoards(foundLead, true);
        } else {
            readThePostedPage(foundLead, true);
        }
    });

    connect(brainReply, &JobScoutReply::failed, this,
            [this, brainReply, jobLead, whyEarlierStepsMissed](const QString &whyItFailed) {
        brainReply->deleteLater();
        finishLeadWith(keepWhatTheUserGaveUs(jobLead,
            QStringLiteral("%1 %2 searched the web too — %3")
                .arg(whyEarlierStepsMissed, brainJobFinder->brainDisplayName(),
                     whyItFailed)));
    });
}

void JobScout::keepTheRealPosting(const JobLead &jobLead, const JobPosting &realPosting,
                                  bool theBrainFoundTheLink, bool onlyThePageTitleWasRead)
{
    QString whatHappened = storeOnePostingAndSayWhatHappened(
        realPosting, AtsBoardName::displayNameFor(realPosting.postingSource));

    if (lastHandAddedJobPostingId != 0) {
        // The link the user pasted is a real place to apply, and it is often
        // not the one the search found. Paste a Lever link for a company that
        // is also on Ashby, and the search answers from Ashby. Both links
        // work, so both are kept, and the user picks which one to use.
        if (jobLead.boardIdentity.isKnown()
                && jobLead.boardIdentity.boardName != realPosting.postingSource) {
            rememberRouteToThisJob(lastHandAddedJobPostingId,
                                   jobLead.boardIdentity.boardName,
                                   jobLead.boardIdentity.jobId,
                                   jobLead.discoveryUrl);
        }
    }

    if (theBrainFoundTheLink) {
        whatHappened = QStringLiteral("%1 found the link and Job Crush read the posting. %2")
            .arg(brainJobFinder->brainDisplayName(), whatHappened);
    }
    if (onlyThePageTitleWasRead) {
        whatHappened += QStringLiteral(" Only the page title could be read, so open the "
                                       "job and check the details.");
    }
    finishLeadWith(whatHappened);
}

// Saves the job exactly as the user gave it, because the search for the real
// posting found nothing.
QString JobScout::keepWhatTheUserGaveUs(const JobLead &jobLead,
                                        const QString &whyTheRealOneIsMissing)
{
    if (jobLead.positionTitle.trimmed().isEmpty()
            || jobLead.companyName.trimmed().isEmpty()) {
        // Nothing to save. A link with no title and no company is not a job.
        return QStringLiteral(
            "Job Crush couldn't get the job off that link. %1 Type the company name "
            "and the job title instead and it will go look again.")
            .arg(whyTheRealOneIsMissing);
    }

    JobPosting leadAsPosting;
    leadAsPosting.companyName = jobLead.companyName;
    leadAsPosting.positionTitle = jobLead.positionTitle;
    leadAsPosting.locationText = jobLead.locationText;
    leadAsPosting.salaryText = jobLead.salaryText;
    leadAsPosting.sourceUrl = jobLead.discoveryUrl;
    leadAsPosting.fullDescriptionText = jobLead.rawText;
    leadAsPosting.isRemoteRole = jobLead.isRemoteRole;
    leadAsPosting.postedTimestamp = jobLead.postedTimestamp;
    // Nothing was read off a board, so the posting has no board. The lead may
    // name one, because the link pointed at Greenhouse, say. Recording that
    // board here would tell the user Job Crush found the job there, and it
    // did not.
    leadAsPosting.postingSource.clear();

    // A hand-added job has no id from a board, so one is made from its company
    // and title. The repository finds duplicates by source and id, so without
    // this, pasting the same job twice would store it twice.
    leadAsPosting.externalSourceId = crossSourceIdentityOf(leadAsPosting);

    return storeOnePostingAndSayWhatHappened(leadAsPosting, QString())
        + QLatin1Char(' ') + whyTheRealOneIsMissing;
}

void JobScout::rememberRouteToThisJob(qint64 jobPostingId, const QString &boardName,
                                      const QString &externalId,
                                      const QString &postingUrl)
{
    PostingSource route;
    route.jobPostingId = jobPostingId;
    route.boardName    = boardName;
    route.externalId   = externalId;
    route.postingUrl   = postingUrl;

    // If this fails, the card is missing one link. The job itself is already
    // saved, so the failure is logged and the work goes on.
    if (!postingSourceRepository.recordPostingSourceIfNew(route)) {
        qWarning().noquote() << QStringLiteral(
            "JobScout::rememberRouteToThisJob: could not record %1 for job posting "
            "%2 — %3").arg(boardName).arg(jobPostingId)
                      .arg(postingSourceRepository.lastErrorText());
    }
}

QString JobScout::storeOnePostingAndSayWhatHappened(JobPosting jobPosting,
                                                    const QString &boardItCameFrom)
{
    if (jobPosting.discoveredTimestamp.isNull()) {
        jobPosting.discoveredTimestamp = QDateTime::currentDateTime();
    }

    // Only the hand-add steps call this function, so every posting that
    // reaches it was added by the user. This is the one place that marks it.
    jobPosting.scoutSource = ScoutSourceText::You;

    bool wasAlreadyKnown = false;
    if (!jobPostingRepository.insertDiscoveryIfNew(jobPosting, wasAlreadyKnown)) {
        lastHandAddedJobPostingId = 0;
        return QStringLiteral("Job Crush couldn't save that job — %1. Try again.")
            .arg(jobPostingRepository.lastErrorText());
    }

    // Remembered so the page can take the user straight to the row.
    lastHandAddedJobPostingId = jobPosting.jobPostingId;

    // Job Crush already had this job, from a sweep or an earlier paste. The
    // user just added it by hand, so it goes on the Manual Add list now.
    // This must happen before discoveriesChanged below, or the list is
    // rebuilt without it.
    if (wasAlreadyKnown
            && !jobPostingRepository.markJobPostingAsHandAdded(jobPosting.jobPostingId)) {
        // Logged by the repository. The job is saved, but the user is told
        // the truth: it will not be on the Manual Add list.
        lastHandAddedJobPostingId = 0;
        return QStringLiteral("You already had this job, but Job Crush couldn't move it "
                              "to Manual Add — %1. Look for it on Top Prospects.")
            .arg(jobPostingRepository.lastErrorText());
    }

    // The place this posting was read off. Recorded even when it is the only
    // one, because every card builds its links from this list, and a job with
    // no recorded place would show no link at all.
    rememberRouteToThisJob(jobPosting.jobPostingId, jobPosting.postingSource,
                           jobPosting.externalSourceId, jobPosting.sourceUrl);

    emit discoveriesChanged();

    const QString jobDescribed = jobPosting.companyName.isEmpty()
        ? QStringLiteral("\"%1\"").arg(jobPosting.positionTitle)
        : QStringLiteral("\"%1\" at %2").arg(jobPosting.positionTitle, jobPosting.companyName);

    if (wasAlreadyKnown) {
        return QStringLiteral("You already had this one. %1 is on your Manual Add list.")
            .arg(jobDescribed);
    }
    if (boardItCameFrom.isEmpty()) {
        return QStringLiteral("Saved %1 to Manual Add.").arg(jobDescribed);
    }
    return QStringLiteral("Found it on %1. %2 is now in Manual Add.")
        .arg(boardItCameFrom, jobDescribed);
}

void JobScout::finishLeadWith(const QString &statusTextForTheUser)
{
    aLeadIsBeingResolved = false;
    storedLeadStatusText = statusTextForTheUser;
    emit leadStatusChanged();
}

void JobScout::sayThisAboutTheLead(const QString &statusTextForTheUser)
{
    storedLeadStatusText = statusTextForTheUser;
    emit leadStatusChanged();
}
