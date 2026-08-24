#include "JobScout.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "../../model/JobPostingRepository.h"
#include "ArbeitnowJobSource.h"
#include "AtsBoardDetector.h"
#include "CanonicalPostingResolver.h"
#include "FollowedEmployerJobSource.h"
#include "FollowedEmployerRoster.h"
#include "JobicyJobSource.h"
#include "UsaJobsJobSource.h"
#include "JobLead.h"
#include "JobScoutReply.h"
#include "JobSearchProfile.h"
#include "JobSourceProvider.h"
#include "JobSourceRoster.h"
#include "RemotiveJobSource.h"

namespace {

// Two boards can carry the same job. This is the identity Job Crush uses to
// recognize that on the Top Prospects list: company and title, stripped down
// to letters and numbers so punctuation and spacing differences between
// boards don't create a phantom second listing.
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
                   JobSourceRoster &sourceRoster,
                   FollowedEmployerRoster &followedEmployerRoster,
                   JobSearchProfile &searchProfile,
                   const QString &diagnosticsFolderPath,
                   QObject *parent)
    : QObject(parent)
    , sweepLogFolderPath(diagnosticsFolderPath)
    , discoveredJobPostingRepository(jobPostingRepository)
    , registeredSourceRoster(sourceRoster)
    , watchedEmployerRoster(followedEmployerRoster)
    , userSearchProfile(searchProfile)
{
    // Every site whose client is written, built once and kept. Adding a site
    // is one line here plus its class — nothing above JobScout changes.
    builtJobSourceProviders.push_back(
        std::make_unique<FollowedEmployerJobSource>(watchedEmployerRoster));
    builtJobSourceProviders.push_back(std::make_unique<RemotiveJobSource>());
    builtJobSourceProviders.push_back(std::make_unique<ArbeitnowJobSource>());
    builtJobSourceProviders.push_back(std::make_unique<JobicyJobSource>());
    builtJobSourceProviders.push_back(std::make_unique<UsaJobsJobSource>(registeredSourceRoster));

    employerBoardResolver = std::make_unique<CanonicalPostingResolver>(this);

    // Editing the profile re-ranks everything already stored — instantly and
    // for free, because the scorer is arithmetic rather than a paid call.
    connect(&userSearchProfile, &JobSearchProfile::searchProfileChanged,
            this, &JobScout::discoveriesChanged);

    // Ticking a site changes which tabs exist and what a sweep covers.
    connect(&registeredSourceRoster, &JobSourceRoster::enabledSourcesChanged,
            this, &JobScout::discoveriesChanged);

    connect(&watchedEmployerRoster, &FollowedEmployerRoster::followedEmployersChanged,
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
    return nullptr; // this site's client has not been written yet
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
        return; // one at a time, so the numbers on screen always mean something
    }

    const QStringList enabledSourceNames = registeredSourceRoster.enabledSourceStorageNames();
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

    // Fill the running list BEFORE firing anything: a site that answers from
    // cache could otherwise finish while the list still looked empty, and the
    // sweep would report itself done before it started.
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

    JobScoutReply *scoutReply = sourceProvider->searchForJobs(userSearchProfile, this);

    connect(scoutReply, &JobScoutReply::finished, this,
            [this, scoutReply, sourceStorageName, sourceDisplayName](
                const QList<JobPosting> &foundJobPostings) {
        const int newJobCountBefore = totalNewJobsThisSweep;
        recordFindingsFromSource(sourceStorageName, foundJobPostings);
        const int newJobsFromThisSource = totalNewJobsThisSweep - newJobCountBefore;

        // A source that answers politely with nothing at all is not an error,
        // but it is not success either — say which, so an empty tab is never
        // a mystery.
        const bool sourceCameBackEmpty = foundJobPostings.isEmpty();

        // Jobs arrived but Job Crush could not keep them. That is a bug in
        // Job Crush, not a problem with the site, and it says so.
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
        // One site being down is not the sweep failing. Say what happened,
        // keep the others running, and move on.
        //
        // Not every "failure" is trouble. A site that asks to be left alone
        // for an hour and is being left alone is working exactly as intended,
        // and painting that red would send the user looking for a fault that
        // is not there.
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

void JobScout::recordFindingsFromSource(const QString &sourceStorageName,
                                        const QList<JobPosting> &foundJobPostings)
{
    Q_UNUSED(sourceStorageName);

    jobPostingsRefusedThisSource = 0;
    firstRefusalReasonThisSource.clear();

    for (JobPosting foundJobPosting : foundJobPostings) {
        bool wasAlreadyKnown = false;
        if (!discoveredJobPostingRepository.insertDiscoveryIfNew(foundJobPosting,
                                                                 wasAlreadyKnown)) {
            // One bad row must not sink the sweep — but it must not vanish
            // either. A source whose every row is refused looks EXACTLY like
            // a source that found nothing, and that mistake cost days.
            ++jobPostingsRefusedThisSource;
            if (firstRefusalReasonThisSource.isEmpty()) {
                firstRefusalReasonThisSource = discoveredJobPostingRepository.lastErrorText();
            }
            continue;
        }
        ++totalJobsFoundThisSweep;
        if (!wasAlreadyKnown) {
            ++totalNewJobsThisSweep;
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

        // The whole reason this exists: whatever went wrong survives the end
        // of the sweep instead of being overwritten by a tidy summary.
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
    const ProspectScorer prospectScorer(userSearchProfile);

    QList<ScoredJobPosting> scoredJobPostings;
    const QList<JobPosting> storedJobPostings =
        discoveredJobPostingRepository.loadJobPostingsFromSource(sourceStorageName);

    scoredJobPostings.reserve(storedJobPostings.count());
    for (const JobPosting &storedJobPosting : storedJobPostings) {
        const bool jobIsInsideSearchArea =
            prospectScorer.jobPostingIsInsideSearchArea(storedJobPosting);
        if (jobIsInsideSearchArea != (searchAreaScope == SearchAreaScope::InsideSearchArea)) {
            continue;
        }
        // A site's own tab stays in the site's own order — newest first. The
        // score rides along so a row can still show it, but sorting by match
        // is what the Top Prospects tab is FOR.
        scoredJobPostings.append(
            { storedJobPosting, prospectScorer.scoreJobPosting(storedJobPosting) });
    }
    return scoredJobPostings;
}

int JobScout::jobPostingCountOutsideSearchArea() const
{
    if (!searchAreaIsNarrowed()) {
        return 0; // nothing is being filtered, so nothing is being held back
    }

    const ProspectScorer prospectScorer(userSearchProfile);
    int heldBackCount = 0;
    for (const JobPosting &discoveredJobPosting :
             discoveredJobPostingRepository.loadAllDiscoveredJobPostings()) {
        if (!prospectScorer.jobPostingIsInsideSearchArea(discoveredJobPosting)) {
            ++heldBackCount;
        }
    }
    return heldBackCount;
}

bool JobScout::searchAreaIsNarrowed() const
{
    return !userSearchProfile.preferredWorkLocations().isEmpty();
}

bool JobScout::searchProfileCanRank() const
{
    return userSearchProfile.hasEnoughToRankBy();
}

QList<ScoredJobPosting> JobScout::rankedTopProspects(SearchAreaScope searchAreaScope) const
{
    const ProspectScorer prospectScorer(userSearchProfile);

    // Best of each job, keyed by who it is with and what it is called, so a
    // posting carried by two sites appears once rather than twice.
    QHash<QString, ScoredJobPosting> bestScoredJobPostingByIdentity;

    const QList<JobPosting> allDiscoveredJobPostings =
        discoveredJobPostingRepository.loadAllDiscoveredJobPostings();

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
        // Equal matches: the fresher posting is the better bet.
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
        return; // a diagnostic that fails must never take the sweep down with it
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
        // Not finishLeadWith. Saying "still busy" must not also say "done" —
        // that would switch the buttons back on and let a second search start
        // beside the first, and then a third.
        sayThisAboutTheLead(QStringLiteral(
            "Job Crush is still looking for the last one. Give it a moment."));
        return;
    }

    JobLead jobLead;
    jobLead.discoveryUrl = trimmedLink;

    AtsBoardDetector boardDetector;
    jobLead.boardIdentity = boardDetector.identify(trimmedLink);

    // Where it came from is worth keeping for the life of the job. A link off
    // a site Job Crush is not allowed to read is still where the user found
    // it, and "where did this come from?" is a question they will ask.
    jobLead.discoverySource = jobLead.boardIdentity.isKnown()
        ? jobLead.boardIdentity.boardName
        : QStringLiteral("pasted");

    goLookForOneLead(jobLead);
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
        // Not finishLeadWith. Saying "still busy" must not also say "done" —
        // that would switch the buttons back on and let a second search start
        // beside the first, and then a third.
        sayThisAboutTheLead(QStringLiteral(
            "Job Crush is still looking for the last one. Give it a moment."));
        return;
    }

    JobLead jobLead;
    jobLead.companyName = trimmedCompany;
    jobLead.positionTitle = trimmedTitle;
    jobLead.discoverySource = QStringLiteral("typed in");

    goLookForOneLead(jobLead);
}

void JobScout::goLookForOneLead(const JobLead &jobLead)
{
    aLeadIsBeingResolved = true;
    storedLeadStatusText = jobLead.companyName.isEmpty()
        ? QStringLiteral("Looking for that job on the employer's own board…")
        : QStringLiteral("Looking for that job on %1's own board…").arg(jobLead.companyName);
    emit leadStatusChanged();

    JobScoutReply *resolverReply = employerBoardResolver->resolve(jobLead, this);

    connect(resolverReply, &JobScoutReply::finished, this,
            [this, resolverReply, jobLead](const QList<JobPosting> &foundPostings) {
        resolverReply->deleteLater();

        if (!foundPostings.isEmpty()) {
            const JobPosting realPosting = foundPostings.first();
            finishLeadWith(storeOnePostingAndSayWhatHappened(
                realPosting,
                AtsBoardName::displayNameFor(realPosting.discoverySource)));
            return;
        }

        // Nobody's board had it.
        finishLeadWith(keepWhatTheUserGaveUs(jobLead, QStringLiteral(
            "%1 isn't on Greenhouse, Lever or Ashby, so open the link to read the "
            "whole posting.").arg(jobLead.companyName)));
    });

    connect(resolverReply, &JobScoutReply::failed, this,
            [this, resolverReply, jobLead](const QString &whyItFailed) {
        resolverReply->deleteLater();
        finishLeadWith(keepWhatTheUserGaveUs(jobLead, whyItFailed));
    });
}

// Saves the job exactly as the user handed it over, because the search for the
// real posting came up empty. A paste that appears to do nothing is worse than
// a plain no.
QString JobScout::keepWhatTheUserGaveUs(const JobLead &jobLead,
                                        const QString &whyTheRealOneIsMissing)
{
    if (jobLead.positionTitle.trimmed().isEmpty()
            || jobLead.companyName.trimmed().isEmpty()) {
        return QStringLiteral(
            "Job Crush couldn't read that link, and it isn't on Greenhouse, Lever or "
            "Ashby. Type the company name and the job title instead and it will go "
            "look again.");
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
    leadAsPosting.discoverySource = jobLead.discoverySource;

    // A hand-added job has no id from a board, so make one out of what it does
    // have. Without this, pasting the same job twice would store it twice: the
    // repository tells duplicates apart by source and id.
    leadAsPosting.externalSourceId = crossSourceIdentityOf(leadAsPosting);

    return storeOnePostingAndSayWhatHappened(leadAsPosting, QString())
        + QLatin1Char(' ') + whyTheRealOneIsMissing;
}

QString JobScout::storeOnePostingAndSayWhatHappened(JobPosting jobPosting,
                                                    const QString &boardItCameFrom)
{
    if (jobPosting.discoveredTimestamp.isNull()) {
        jobPosting.discoveredTimestamp = QDateTime::currentDateTime();
    }

    bool wasAlreadyKnown = false;
    if (!discoveredJobPostingRepository.insertDiscoveryIfNew(jobPosting, wasAlreadyKnown)) {
        return QStringLiteral("Job Crush couldn't save that job — %1. Try again.")
            .arg(discoveredJobPostingRepository.lastErrorText());
    }

    emit discoveriesChanged();

    const QString jobDescribed = jobPosting.companyName.isEmpty()
        ? QStringLiteral("\"%1\"").arg(jobPosting.positionTitle)
        : QStringLiteral("\"%1\" at %2").arg(jobPosting.positionTitle, jobPosting.companyName);

    if (wasAlreadyKnown) {
        return QStringLiteral("You already have this one. %1 is in Discoveries.")
            .arg(jobDescribed);
    }
    if (boardItCameFrom.isEmpty()) {
        return QStringLiteral("Added %1 to Discoveries.").arg(jobDescribed);
    }
    return QStringLiteral("Found it on %1. %2 is now in Discoveries.")
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
