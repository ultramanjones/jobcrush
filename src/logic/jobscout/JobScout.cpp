#include "JobScout.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
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

// During a sweep, discoveriesChanged is sent at most once in this many
// milliseconds, however many companies answer. Each send makes the page
// rebuild its rows, and hundreds of rebuilds would freeze it.
constexpr int discoveriesChangedPauseMilliseconds = 1000;

// Goes between one site's piece and the next in the progress, trouble and
// notice text.
const QString sourceLineSeparator = QStringLiteral("   ·   ");

} // namespace

JobScout::JobScout(JobPostingRepository &jobPostingRepository,
                   PostingSourceRepository &postingSourceRepository,
                   JobSourceRoster &jobSourceRoster,
                   FollowedEmployerRoster &followedEmployerRoster,
                   JobSearchProfile &searchProfile,
                   AiBrain &aiBrain,
                   const QString &diagnosticsFolderPath,
                   const QString &databaseFilePath,
                   QObject *parent)
    : QObject(parent)
    , sweepLogFolderPath(diagnosticsFolderPath)
    , jobPostingRepository(jobPostingRepository)
    , postingSourceRepository(postingSourceRepository)
    , jobSourceRoster(jobSourceRoster)
    , followedEmployerRoster(followedEmployerRoster)
    , searchProfile(searchProfile)
{
    // Build one client for each job site, and keep it. To add a new site,
    // write its client class, add one line here, and set clientIsBuilt to
    // true for that site in the catalog in JobSourceDescriptor.h. The view
    // models and pages do not change.
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

    // When the user edits the search profile, the page asks for its rows
    // again, and every saved job is scored again. This costs no money,
    // because the scorer does plain arithmetic. It does not call a paid AI
    // service.
    connect(&searchProfile, &JobSearchProfile::searchProfileChanged,
            this, &JobScout::discoveriesChanged);

    // When the user ticks or unticks a site in Settings, the page asks for
    // its rows again. Each ticked site has its own tab, so the set of tabs
    // changes, and so does the list of sites the next sweep asks.
    connect(&jobSourceRoster, &JobSourceRoster::enabledSourcesChanged,
            this, &JobScout::discoveriesChanged);

    connect(&followedEmployerRoster, &FollowedEmployerRoster::followedEmployersChanged,
            this, &JobScout::discoveriesChanged);

    // Set up the background thread that saves and scores jobs. The three
    // types are registered so Qt can copy them between threads in queued
    // calls and signals. The DiscoveryWorker is moved onto the background
    // thread. It opens its own database connection there, because Qt lets a
    // connection be used only on the thread that opened it. When the thread
    // stops, the worker is deleted on that thread.
    qRegisterMetaType<DiscoveryRowRequest>();
    qRegisterMetaType<DiscoveryRows>();
    qRegisterMetaType<FindingsSaveOutcome>();

    discoveryWorker = new DiscoveryWorker(databaseFilePath);
    discoveryWorker->moveToThread(&discoveryThread);
    connect(&discoveryThread, &QThread::finished, discoveryWorker, &QObject::deleteLater);
    connect(discoveryWorker, &DiscoveryWorker::findingsSaved,
            this, &JobScout::whenFindingsWereSaved);
    connect(discoveryWorker, &DiscoveryWorker::jobsMissingFromBoards,
            this, &JobScout::whenJobsAreMissingFromBoards);
    connect(discoveryWorker, &DiscoveryWorker::rowsPrepared,
            this, &JobScout::rowsReady);
    discoveryThread.start();
    QMetaObject::invokeMethod(discoveryWorker, [worker = discoveryWorker]() {
        worker->openDatabase();
    });

    discoveriesChangedTimer.setSingleShot(true);
    discoveriesChangedTimer.setInterval(discoveriesChangedPauseMilliseconds);
    connect(&discoveriesChangedTimer, &QTimer::timeout, this, &JobScout::discoveriesChanged);
}

JobScout::~JobScout()
{
    // Stop the background thread, but only after it finishes the work
    // already sent to it. That way found jobs that are waiting to be saved
    // still get saved. To do that, the call to quit() is queued behind that
    // work, as one more task. If this code called quit() directly, the
    // thread would stop and drop the waiting work. When the thread stops, it
    // deletes the worker.
    QMetaObject::invokeMethod(discoveryWorker, [thread = &discoveryThread]() {
        thread->quit();
    });
    discoveryThread.wait();
}

int JobScout::requestRows(const DiscoveryRowRequest &rowRequest)
{
    const int requestNumber = ++lastRowRequestNumber;
    const ScoringProfile scoringProfile = searchProfile.scoringProfile();
    QMetaObject::invokeMethod(discoveryWorker,
                              [worker = discoveryWorker, requestNumber, rowRequest,
                               scoringProfile]() {
        worker->prepareRows(requestNumber, rowRequest, scoringProfile);
    });
    return requestNumber;
}

void JobScout::sayDiscoveriesChangedSoon()
{
    if (!discoveriesChangedTimer.isActive()) {
        discoveriesChangedTimer.start();
    }
}

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
        const SourceSweepProgress progress = sourceSweepProgress.value(sourceStorageName);
        const int jobsSavedSoFar = progress.saveTotals.jobsSaved;
        progressPieces.append(jobsSavedSoFar > 0
            ? QStringLiteral("%1: %2 saved so far…")
                  .arg(progress.sourceDisplayName).arg(jobsSavedSoFar)
            : QStringLiteral("%1: looking…").arg(progress.sourceDisplayName));
    }
    return progressPieces.join(sourceLineSeparator);
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
    sourceSweepProgress.clear();
    sourcesThatHadTroubleThisSweep.clear();
    sourceNoticesThisSweep.clear();
    totalJobsFoundThisSweep = 0;
    totalNewJobsThisSweep = 0;
    storedLastSweepSummaryText.clear();
    storedLastSweepTroubleText.clear();

    // Put every site in sourcesStillSweeping BEFORE starting any of them. A
    // site that answers from its cache can finish right away, inside the
    // call that starts it. If the list were still empty at that moment, the
    // sweep would report that it was done before the other sites started.
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
    SourceSweepProgress &progress = sourceSweepProgress[sourceStorageName];
    progress.sourceDisplayName = descriptorFound ? descriptor.displayName : sourceStorageName;

    JobScoutReply *scoutReply = sourceProvider->searchForJobs(searchProfile, this);

    // Some sites hear from many companies. That kind of site sends each
    // company's jobs as soon as they arrive, through someJobsFound. Each
    // batch is saved right away, one company at a time.
    connect(scoutReply, &JobScoutReply::someJobsFound, this,
            [this, sourceStorageName](const QList<JobPosting> &someFoundJobPostings) {
        saveFindingsInTheBackground(sourceStorageName, someFoundJobPostings);
    });

    connect(scoutReply, &JobScoutReply::finished, this,
            [this, scoutReply, sourceStorageName](const QList<JobPosting> &foundJobPostings) {
        SourceSweepProgress &progress = sourceSweepProgress[sourceStorageName];
        progress.replyHasFinished = true;
        progress.jobsArrived = foundJobPostings.count();

        // If the site already sent its jobs one company at a time, those
        // jobs are already being saved, so they are not saved again.
        // Otherwise, all of the site's jobs are saved now, in one batch.
        if (!scoutReply->deliveredJobsAsTheyArrived()) {
            saveFindingsInTheBackground(sourceStorageName, foundJobPostings);
        }

        // If the site read some companies' whole job boards, ask the
        // background thread to find saved jobs from those companies that
        // were not on their boards this time. Those jobs may be gone. This
        // task is queued after the save tasks, so it runs after every job
        // from this sweep is saved.
        const QStringList employersReadInFull = scoutReply->employersReadInFull();
        if (!employersReadInFull.isEmpty()) {
            ++progress.backgroundTasksPending;
            QMetaObject::invokeMethod(discoveryWorker,
                                      [worker = discoveryWorker, sourceStorageName,
                                       employersReadInFull, foundJobPostings]() {
                worker->findJobsMissingFromBoardsReadInFull(sourceStorageName,
                                                            employersReadInFull,
                                                            foundJobPostings);
            });
        }

        scoutReply->deleteLater();
        finishSourceIfEverythingIsDone(sourceStorageName);
    });

    connect(scoutReply, &JobScoutReply::failed, this,
            [this, scoutReply, sourceStorageName](const QString &humanReadableReason,
                                                  bool sourceHadTrouble) {
        // One site failing does not stop the sweep. Record what it said and
        // let the other sites finish.
        SourceSweepProgress &progress = sourceSweepProgress[sourceStorageName];
        progress.replyHasFinished = true;
        progress.replyFailed = true;
        progress.failureReason = humanReadableReason;
        progress.failureIsTrouble = sourceHadTrouble;

        scoutReply->deleteLater();
        finishSourceIfEverythingIsDone(sourceStorageName);
    });
}

void JobScout::saveFindingsInTheBackground(const QString &sourceStorageName,
                                           const QList<JobPosting> &foundJobPostings)
{
    if (foundJobPostings.isEmpty()) {
        return;
    }
    ++sourceSweepProgress[sourceStorageName].backgroundTasksPending;
    QMetaObject::invokeMethod(discoveryWorker,
                              [worker = discoveryWorker, sourceStorageName,
                               foundJobPostings]() {
        worker->saveFindings(sourceStorageName, foundJobPostings);
    });
}

void JobScout::whenFindingsWereSaved(const QString &sourceStorageName,
                                     const FindingsSaveOutcome &saveOutcome)
{
    SourceSweepProgress &progress = sourceSweepProgress[sourceStorageName];
    --progress.backgroundTasksPending;
    progress.saveTotals.jobsSaved += saveOutcome.jobsSaved;
    progress.saveTotals.newJobs += saveOutcome.newJobs;
    progress.saveTotals.jobsRefused += saveOutcome.jobsRefused;
    if (progress.saveTotals.firstRefusalReason.isEmpty()) {
        progress.saveTotals.firstRefusalReason = saveOutcome.firstRefusalReason;
    }
    totalJobsFoundThisSweep += saveOutcome.jobsSaved;
    totalNewJobsThisSweep += saveOutcome.newJobs;

    // If the batch had new jobs, tell the page soon, so the new jobs show
    // up while the sweep is still running.
    if (saveOutcome.newJobs > 0) {
        sayDiscoveriesChangedSoon();
    }
    emit sweepProgressChanged();
    finishSourceIfEverythingIsDone(sourceStorageName);
}

void JobScout::whenJobsAreMissingFromBoards(const QString &sourceStorageName,
                                            const QList<JobPosting> &missingJobPostings)
{
    --sourceSweepProgress[sourceStorageName].backgroundTasksPending;
    sayDiscoveriesChangedSoon();
    finishSourceIfEverythingIsDone(sourceStorageName);

    // One missing answer is not enough to call a job gone, so each missing
    // job is checked again. Job Crush asks the company's board two
    // questions: one about the job's own page, and one about the company's
    // list of jobs. The job is marked gone only if both answers say it is
    // gone. These checks run after this site is marked finished in the
    // sweep. They do not change the sweep's counts. They only change which
    // jobs show the amber "gone" mark.
    const AtsBoardDetector boardDetector;
    for (const JobPosting &missingJobPosting : missingJobPostings) {
        const AtsBoardIdentity identity = boardDetector.identify(missingJobPosting.sourceUrl);
        const qint64 jobPostingId = missingJobPosting.jobPostingId;
        ListingCheckReply *checkReply =
            goneConfirmer.checkOneJob(identity, missingJobPosting.positionTitle, this);
        connect(checkReply, &ListingCheckReply::answered, this,
                [this, jobPostingId](ListingAnswer listingAnswer, const QString &whatHappened) {
            if (listingAnswer != ListingAnswer::Gone) {
                qInfo().noquote() << QStringLiteral(
                    "JobScout::whenJobsAreMissingFromBoards: job posting %1 was missing "
                    "from its company's board, but "
                    "it is not marked gone — %2").arg(jobPostingId).arg(whatHappened);
                return;
            }
            jobPostingRepository.updatePostingGoneSince(jobPostingId,
                                                        QDateTime::currentDateTime());
            sayDiscoveriesChangedSoon();
        });
    }
}

void JobScout::finishSourceIfEverythingIsDone(const QString &sourceStorageName)
{
    if (!sourcesStillSweeping.contains(sourceStorageName)) {
        return; // already finished
    }
    const SourceSweepProgress progress = sourceSweepProgress.value(sourceStorageName);
    if (!progress.replyHasFinished || progress.backgroundTasksPending > 0) {
        return;
    }

    const QString &displayName = progress.sourceDisplayName;
    const FindingsSaveOutcome &totals = progress.saveTotals;

    if (progress.replyFailed) {
        // Not every failure is a problem. For example, a site may ask Job
        // Crush to wait before asking again. While Job Crush waits, the site
        // is working as it should. If the reply said the failure is not a
        // problem, it is shown as a notice, not as an error.
        const QString outcomeText =
            QStringLiteral("%1: %2").arg(displayName, progress.failureReason);
        if (!progress.failureIsTrouble) {
            sourceNoticesThisSweep.append(outcomeText);
        }
        finishSourceAndReportProgress(sourceStorageName, outcomeText, progress.failureIsTrouble);
        return;
    }

    // If a site answers with no jobs, that is not an error, but it is not a
    // success either. The site's outcome line says it sent no jobs, so the
    // user knows why that site's tab is empty. The site is also listed in
    // the trouble text.
    const bool sourceCameBackEmpty = progress.jobsArrived == 0;

    // If jobs arrived but the database refused every one, the problem is in
    // Job Crush, not in the site. The outcome line says so.
    const bool everyJobWasRefused = !sourceCameBackEmpty
        && totals.jobsRefused == progress.jobsArrived;

    QString outcomeText;
    if (sourceCameBackEmpty) {
        outcomeText = QStringLiteral("%1: answered, but sent no jobs back").arg(displayName);
    } else if (everyJobWasRefused) {
        outcomeText = QStringLiteral("%1: %2 jobs arrived but Job Crush could not store any "
                                     "of them — %3")
                          .arg(displayName)
                          .arg(progress.jobsArrived)
                          .arg(totals.firstRefusalReason);
    } else if (totals.jobsRefused > 0) {
        outcomeText = QStringLiteral("%1: %2 found, %3 new, %4 could not be stored — %5")
                          .arg(displayName)
                          .arg(progress.jobsArrived)
                          .arg(totals.newJobs)
                          .arg(totals.jobsRefused)
                          .arg(totals.firstRefusalReason);
    } else {
        outcomeText = QStringLiteral("%1: %2 found, %3 new")
                          .arg(displayName)
                          .arg(progress.jobsArrived)
                          .arg(totals.newJobs);
    }

    finishSourceAndReportProgress(sourceStorageName, outcomeText,
                                  sourceCameBackEmpty || totals.jobsRefused > 0);
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

        // Keep the trouble text and the notice text after the sweep ends.
        // The summary does not replace them.
        storedLastSweepTroubleText =
            sourcesThatHadTroubleThisSweep.join(sourceLineSeparator);
        storedLastSweepNoticeText =
            sourceNoticesThisSweep.join(sourceLineSeparator);

        writeSweepLog();

        // The sweep is over, so tell the page to refresh now. Stop the
        // one-second timer first, so the page does not refresh again a
        // second later.
        discoveriesChangedTimer.stop();
        emit discoveriesChanged();
    }

    emit sweepProgressChanged();
}

qint64 JobScout::mostRecentlyHandAddedJobPostingId() const
{
    return lastHandAddedJobPostingId;
}

bool JobScout::searchAreaIsNarrowed() const
{
    return !searchProfile.preferredWorkLocations().isEmpty();
}

bool JobScout::searchProfileCanRank() const
{
    return searchProfile.hasEnoughToRankBy();
}

void JobScout::writeSweepLog() const
{
    if (sweepLogFolderPath.isEmpty()) {
        return;
    }
    QDir().mkpath(sweepLogFolderPath);

    QFile sweepLogFile(QDir(sweepLogFolderPath).filePath(QStringLiteral("lastSweep.log")));
    if (!sweepLogFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        // The log file could not be opened. The sweep itself worked, so its
        // results are kept. Only a warning is written.
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
    // Reading pasted text does not need a web search, so any connected brain
    // can do it. That includes Ollama, which runs on the user's own computer
    // and cannot search the web.
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

    // Start the same search as for any job added by hand. The form always
    // has a company and a title, so the search starts on the employer's own
    // job board. If that finds nothing, the AI brain is asked, if it can
    // search the web. If nothing finds the job, the job is saved with what
    // the user left in the form.
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
        emit lookingElsewhereFinished(jobPostingId,
                                      OtherPlaceSearchResult::JobIsNotInTheDatabase,
                                      QString());
        return;
    }
    if (jobPosting.companyName.trimmed().isEmpty()
            || jobPosting.positionTitle.trimmed().isEmpty()) {
        emit lookingElsewhereFinished(jobPostingId,
                                      OtherPlaceSearchResult::NothingToSearchWith,
                                      QString());
        return; // nothing to search with
    }

    // The place where the job was found is also a place to apply. Record it
    // first, so the job always has at least one place to apply.
    if (!jobPosting.sourceUrl.isEmpty() && !jobPosting.postingSource.isEmpty()) {
        rememberRouteToThisJob(jobPostingId, jobPosting.postingSource,
                               jobPosting.externalSourceId, jobPosting.sourceUrl);
    }

    // If the job's link already points to one job on an employer's own
    // board, there is nothing better to find. Stop here and send no
    // requests.
    if (AtsBoardDetector().identify(jobPosting.sourceUrl).namesOneJob()) {
        emit lookingElsewhereFinished(
            jobPostingId, OtherPlaceSearchResult::AlreadyOnTheEmployersOwnBoard,
            AtsBoardName::displayNameFor(jobPosting.postingSource));
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
            emit lookingElsewhereFinished(jobPostingId,
                                          OtherPlaceSearchResult::NothingElseFound,
                                          QString());
            return;
        }
        const JobPosting &employersOwnPosting = foundPostings.first();
        rememberRouteToThisJob(jobPostingId, employersOwnPosting.postingSource,
                               employersOwnPosting.externalSourceId,
                               employersOwnPosting.sourceUrl);

        // The job was just read off a live board, so any "gone" mark on it is
        // wrong. Clear it. An invalid timestamp is how the mark is cleared.
        jobPostingRepository.updatePostingGoneSince(jobPostingId, QDateTime());

        emit placesToApplyChanged(jobPostingId);
        emit lookingElsewhereFinished(
            jobPostingId, OtherPlaceSearchResult::FoundAnotherPlace,
            AtsBoardName::displayNameFor(employersOwnPosting.postingSource));
    });
    // Finding nothing is a normal answer, but the user asked a question and
    // has to get an answer. The reason is logged too, in case a board has
    // stopped answering.
    connect(resolverReply, &JobScoutReply::failed, this,
            [this, resolverReply, jobPostingId](const QString &whyItFailed) {
        resolverReply->deleteLater();
        qInfo().noquote() << QStringLiteral(
            "JobScout::lookForOtherPlacesToApply: no other place found for job "
            "posting %1 — %2").arg(jobPostingId).arg(whyItFailed);
        emit lookingElsewhereFinished(jobPostingId,
                                      OtherPlaceSearchResult::NothingElseFound,
                                      QString());
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
        // Use sayThisAboutTheLead here, not finishLeadWith. finishLeadWith
        // sets aLeadIsBeingResolved to false. That would turn the add buttons
        // back on and let a second search start while the first one runs.
        sayThisAboutTheLead(QStringLiteral(
            "Job Crush is still looking for the last one. Give it a moment."));
        return;
    }

    JobLead jobLead;
    jobLead.discoveryUrl = trimmedLink;

    AtsBoardDetector boardDetector;
    jobLead.boardIdentity = boardDetector.identify(trimmedLink);

    // postingSource holds only the name of a real job board, such as
    // Greenhouse. If the link does not point to a known board, postingSource
    // stays empty. The fact that the user pasted the link is recorded in a
    // different field, scoutSource, when the job is saved.
    jobLead.postingSource = jobLead.boardIdentity.isKnown()
        ? jobLead.boardIdentity.boardName
        : QString();

    // Job Crush does not open LinkedIn or Indeed links. The link alone does
    // not say which company or which job it is. So if the link is from one
    // of those sites, tell the user now, and tell them what to do instead.
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
        // Use sayThisAboutTheLead here, not finishLeadWith. finishLeadWith
        // sets aLeadIsBeingResolved to false. That would turn the add buttons
        // back on and let a second search start while the first one runs.
        sayThisAboutTheLead(QStringLiteral(
            "Job Crush is still looking for the last one. Give it a moment."));
        return;
    }

    JobLead jobLead;
    jobLead.companyName = trimmedCompany;
    jobLead.positionTitle = trimmedTitle;
    // The user gave no link, so no job board is known yet. The search of the
    // employer boards looks for the company by its name instead.

    beginHuntForOneLead(jobLead);
}

// The search for one job the user added by hand. The steps:
//
//   1. If the link points to a known job board, or the user gave both a
//      company and a job title, search the employer's own job board. This
//      gives the best result. The text is the employer's own words, and the
//      posting is taken down when the job is filled.
//      Otherwise, read the web page at the pasted link. Most job pages put
//      the job's details in a block of JSON-LD data, and the page reader
//      knows how to read it.
//   2. If step 1 finds nothing, ask the AI brain to search the web. This
//      happens only if a brain is connected and it can search the web. The
//      brain is asked only once per job, because each search costs money.
//      The brain answers with a link. If the link points to one job on an
//      employer's board, Job Crush reads it from that board. Otherwise Job
//      Crush reads the web page at the link. That way the saved posting
//      holds the employer's text, not the brain's summary of it.
//
// If every step finds nothing, the job is saved with what the user gave.
// If the user gave only a link, there is nothing to save, and the user is
// asked to type the company and the title instead.
void JobScout::beginHuntForOneLead(const JobLead &jobLead)
{
    aLeadIsBeingResolved = true;
    // Clear the id of the last hand-added job now. The "Show me" button
    // jumps to the row of that job. While this new job is being looked up,
    // the button would otherwise jump to the previous job, not this one.
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
            "or on a Workday or Taleo site it knows.").arg(
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
        // The brain was already asked. It gave a link, but Job Crush could
        // not read the page at that link. The job is still saved, with the
        // brain's link, so the user can open the link and check it.
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

        // Treat the brain's answer as a new JobLead, not as a finished
        // posting. Job Crush reads the page at the brain's link, the same
        // way it reads a pasted link. That way the saved text is the
        // employer's text. If the brain left the title or the company blank,
        // the value the user typed is used instead.
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
        // The job was saved. If the user pasted a link to a different board
        // than the one the posting came from, record that link too. The
        // pasted link is a real place to apply. For example, if the user
        // pastes a Lever link for a company that is also on Ashby, the search
        // may answer from Ashby. Both links work, so both are kept, and the
        // user picks which one to use.
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
        // If the title or the company is missing, there is nothing to save.
        // A link alone is not enough to make a job.
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
    // No job board was read, so leave postingSource empty. The JobLead may
    // name a board, for example if the pasted link pointed at Greenhouse. If
    // that board were recorded here, the user would be told that Job Crush
    // found the job on Greenhouse. That would not be true.
    leadAsPosting.postingSource.clear();

    // A job added by hand has no id from a board. So make an id from its
    // company and title. The repository finds duplicates by matching the
    // source and the id. Without this id, pasting the same job twice would
    // save it twice.
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

    // If this fails, the job's card is missing one link. The job itself is
    // already saved, so a warning is written and the work goes on.
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

    // Only the steps for adding a job by hand call this function. So every
    // posting that comes here was added by the user. This is the only place
    // that sets scoutSource to You.
    jobPosting.scoutSource = ScoutSourceText::You;

    bool wasAlreadyKnown = false;
    if (!jobPostingRepository.insertDiscoveryIfNew(jobPosting, wasAlreadyKnown)) {
        lastHandAddedJobPostingId = 0;
        return QStringLiteral("Job Crush couldn't save that job — %1. Try again.")
            .arg(jobPostingRepository.lastErrorText());
    }

    // Keep the id so the page can jump straight to this job's row.
    lastHandAddedJobPostingId = jobPosting.jobPostingId;

    // If Job Crush already had this job, from a sweep or an earlier paste,
    // mark it as added by hand, so it shows on the Manual Add tab. This must
    // happen before discoveriesChanged is sent below. Otherwise the tab's
    // list would be rebuilt without this job.
    if (wasAlreadyKnown
            && !jobPostingRepository.markJobPostingAsHandAdded(jobPosting.jobPostingId)) {
        // The repository already logged the error. The job is saved, but it
        // will not be on the Manual Add tab. The user is told that, and told
        // where to look instead.
        lastHandAddedJobPostingId = 0;
        return QStringLiteral("You already had this job, but Job Crush couldn't move it "
                              "to Manual Add — %1. Look for it on Top Prospects.")
            .arg(jobPostingRepository.lastErrorText());
    }

    // Record the place this posting was read from. Do this even when it is
    // the only place, because every job card builds its links from this list
    // of places. A job with no recorded place would show no link at all.
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
