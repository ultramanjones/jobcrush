#include "JobScout.h"

#include <algorithm>

#include "../../model/JobPostingRepository.h"
#include "ArbeitnowJobSource.h"
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
                   JobSearchProfile &searchProfile,
                   QObject *parent)
    : QObject(parent)
    , discoveredJobPostingRepository(jobPostingRepository)
    , registeredSourceRoster(sourceRoster)
    , userSearchProfile(searchProfile)
{
    // Every site whose client is written, built once and kept. Adding a site
    // is one line here plus its class — nothing above JobScout changes.
    builtJobSourceProviders.push_back(std::make_unique<RemotiveJobSource>());
    builtJobSourceProviders.push_back(std::make_unique<ArbeitnowJobSource>());

    // Editing the profile re-ranks everything already stored — instantly and
    // for free, because the scorer is arithmetic rather than a paid call.
    connect(&userSearchProfile, &JobSearchProfile::searchProfileChanged,
            this, &JobScout::discoveriesChanged);

    // Ticking a site changes which tabs exist and what a sweep covers.
    connect(&registeredSourceRoster, &JobSourceRoster::enabledSourcesChanged,
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
    totalJobsFoundThisSweep = 0;
    totalNewJobsThisSweep = 0;
    storedLastSweepSummaryText.clear();

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

        finishSourceAndReportProgress(
            sourceStorageName,
            QStringLiteral("%1: %2 found, %3 new")
                .arg(sourceDisplayName)
                .arg(foundJobPostings.count())
                .arg(newJobsFromThisSource));

        scoutReply->deleteLater();
    });

    connect(scoutReply, &JobScoutReply::failed, this,
            [this, scoutReply, sourceStorageName, sourceDisplayName](
                const QString &humanReadableReason) {
        // One site being down is not the sweep failing. Say what happened,
        // keep the others running, and move on.
        finishSourceAndReportProgress(
            sourceStorageName,
            QStringLiteral("%1: %2").arg(sourceDisplayName, humanReadableReason));

        scoutReply->deleteLater();
    });
}

void JobScout::recordFindingsFromSource(const QString &sourceStorageName,
                                        const QList<JobPosting> &foundJobPostings)
{
    Q_UNUSED(sourceStorageName);

    for (JobPosting foundJobPosting : foundJobPostings) {
        bool wasAlreadyKnown = false;
        if (!discoveredJobPostingRepository.insertDiscoveryIfNew(foundJobPosting,
                                                                 wasAlreadyKnown)) {
            continue; // a database failure on one row should not sink the sweep
        }
        ++totalJobsFoundThisSweep;
        if (!wasAlreadyKnown) {
            ++totalNewJobsThisSweep;
        }
    }
}

void JobScout::finishSourceAndReportProgress(const QString &sourceStorageName,
                                             const QString &sourceOutcomeText)
{
    sourcesStillSweeping.removeAll(sourceStorageName);
    finishedSourceOutcomeLines.append(sourceOutcomeText);

    if (sourcesStillSweeping.isEmpty()) {
        storedLastSweepSummaryText =
            QStringLiteral("%1 jobs checked · %2 new · %3 %4")
                .arg(totalJobsFoundThisSweep)
                .arg(totalNewJobsThisSweep)
                .arg(finishedSourceOutcomeLines.count())
                .arg(finishedSourceOutcomeLines.count() == 1
                         ? QStringLiteral("site") : QStringLiteral("sites"));
        emit discoveriesChanged();
    }

    emit sweepProgressChanged();
}

QList<ScoredJobPosting> JobScout::scoredJobPostingsFromSource(
    const QString &sourceStorageName) const
{
    const ProspectScorer prospectScorer(userSearchProfile);

    QList<ScoredJobPosting> scoredJobPostings;
    const QList<JobPosting> storedJobPostings =
        discoveredJobPostingRepository.loadJobPostingsFromSource(sourceStorageName);

    scoredJobPostings.reserve(storedJobPostings.count());
    for (const JobPosting &storedJobPosting : storedJobPostings) {
        // A site's own tab stays in the site's own order — newest first. The
        // score rides along so a row can still show it, but sorting by match
        // is what the Top Prospects tab is FOR.
        scoredJobPostings.append(
            { storedJobPosting, prospectScorer.scoreJobPosting(storedJobPosting) });
    }
    return scoredJobPostings;
}

bool JobScout::searchProfileCanRank() const
{
    return userSearchProfile.hasEnoughToRankBy();
}

QList<ScoredJobPosting> JobScout::rankedTopProspects() const
{
    const ProspectScorer prospectScorer(userSearchProfile);

    // Best of each job, keyed by who it is with and what it is called, so a
    // posting carried by two sites appears once rather than twice.
    QHash<QString, ScoredJobPosting> bestScoredJobPostingByIdentity;

    const QList<JobPosting> allDiscoveredJobPostings =
        discoveredJobPostingRepository.loadAllDiscoveredJobPostings();

    for (const JobPosting &discoveredJobPosting : allDiscoveredJobPostings) {
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
