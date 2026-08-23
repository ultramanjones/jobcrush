#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <memory>
#include <vector>

#include "../../model/JobPosting.h"
#include "ProspectScorer.h"

class JobPostingRepository;
class JobSearchProfile;
class JobSourceProvider;
class JobSourceRoster;

// ScoredJobPosting
//
// A posting together with how it stood up against the profile. Bundled so the
// list travels as one thing instead of two lists the caller has to keep lined
// up — which is the sort of bookkeeping that eventually gets out of step.
struct ScoredJobPosting {
    JobPosting jobPosting;
    ProspectMatchResult matchResult;
};

// JobScout
//
// The front door of job discovery — a ModelView resident, and AIBrain's
// opposite number. Everything above it asks JobScout and never touches a job
// site, an HTTP request, or the scoring arithmetic directly.
//
// Responsibilities, and nothing more:
//  - run a sweep across the sites the user ticked, one reply per site,
//  - hand every find to the repository, which knows what it has seen before,
//  - rank what is stored against the search profile,
//  - report honest progress with real numbers while it works.
//
// THE BOARD IS SACRED: nothing here ever creates a JobApplication. A found
// job reaches the pipeline only when the user hits CRUSH. JobScout finds; the
// human decides.
class JobScout : public QObject {
    Q_OBJECT
public:
    JobScout(JobPostingRepository &jobPostingRepository,
             JobSourceRoster &sourceRoster,
             JobSearchProfile &searchProfile,
             QObject *parent = nullptr);
    ~JobScout() override;

    // True while at least one site is still answering.
    bool sweepIsRunning() const;

    // What is happening, in real numbers — never a spinner, never a lie.
    // "Remotive: 43 found, 11 new · Arbeitnow: looking…"
    QString sweepProgressText() const;

    // How the last finished sweep went, kept on screen afterward so the user
    // can see what a run actually produced.
    QString lastSweepSummaryText() const;

    // Any site that could not answer, and what it said — kept AFTER the sweep
    // ends rather than being replaced by the summary. A source that quietly
    // fails and then erases its own explanation is worse than one that never
    // ran: the user is left knowing something is wrong and unable to say what.
    QString lastSweepTroubleText() const;

    // Sweeps every ticked site. Does nothing while a sweep is already running
    // — one at a time, so the numbers on screen always mean something.
    void startSweep();

    // Everything one site has delivered, newest first, each with its score.
    QList<ScoredJobPosting> scoredJobPostingsFromSource(const QString &sourceStorageName) const;

    // Every discovery from every site, best match first, each job appearing
    // once even when two sites carry it.
    QList<ScoredJobPosting> rankedTopProspects() const;

    // True once the search profile says enough for a ranking to mean
    // something. Asked through JobScout so nothing above has to reach past it
    // to the profile — the front door stays the only door.
    bool searchProfileCanRank() const;

signals:
    // sweepIsRunning() or sweepProgressText() changed.
    void sweepProgressChanged();

    // Stored discoveries changed — the lists above are worth re-reading.
    void discoveriesChanged();

private:
    // The client for a site, or nullptr while that site's client has not been
    // written yet.
    JobSourceProvider *providerFor(const QString &sourceStorageName) const;

    // Wires up one site's reply and folds its results into the sweep.
    void beginSweepOfSource(const QString &sourceStorageName);

    // Stores one site's finds, counting what was genuinely new.
    void recordFindingsFromSource(const QString &sourceStorageName,
                                  const QList<JobPosting> &foundJobPostings);

    void finishSourceAndReportProgress(const QString &sourceStorageName,
                                       const QString &sourceOutcomeText,
                                       bool sourceHadTrouble);

    JobPostingRepository &discoveredJobPostingRepository;
    JobSourceRoster &registeredSourceRoster;
    JobSearchProfile &userSearchProfile;

    // A plain vector rather than a keyed container: QHash is implicitly
    // shared and so needs a copyable value, which a unique_ptr is not. No
    // loss — every provider already knows its own storage name, so looking
    // one up by asking is simpler than keeping a second list of keys in step
    // with this one, and there will only ever be a handful of them.
    std::vector<std::unique_ptr<JobSourceProvider>> builtJobSourceProviders;

    // Live sweep bookkeeping. Cleared when a sweep ends.
    QStringList sourcesStillSweeping;
    QStringList finishedSourceOutcomeLines;
    QStringList sourcesThatHadTroubleThisSweep;
    QString storedLastSweepTroubleText;
    int totalJobsFoundThisSweep = 0;
    int totalNewJobsThisSweep = 0;
    QString storedLastSweepSummaryText;
};
