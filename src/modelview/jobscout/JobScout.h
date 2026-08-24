#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <memory>
#include <vector>

#include "../../model/JobPosting.h"
#include "ProspectScorer.h"

class CanonicalPostingResolver;
class FollowedEmployerRoster;
struct JobLead;
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

// SearchAreaScope
//
// Which side of the location filter a caller wants. Two lists, one set of
// findings — the jobs that sit where the user said they will work, and the
// ones held back because they don't.
//
// OutsideSearchArea exists so those jobs are never simply destroyed. Hiding
// something the user might want, with no way to look at it, is the app making
// the decision for them; keeping the other half addressable means a tab can
// show it whenever they ask.
enum class SearchAreaScope {
    InsideSearchArea,
    OutsideSearchArea
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
    // diagnosticsFolderPath is where the sweep log is written — the app's own
    // data folder, handed in by the composition root so this class never has
    // to know where that is.
    JobScout(JobPostingRepository &jobPostingRepository,
             JobSourceRoster &sourceRoster,
             FollowedEmployerRoster &followedEmployerRoster,
             JobSearchProfile &searchProfile,
             const QString &diagnosticsFolderPath,
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

    // What a site SAID that was not a count and not a fault — most often a
    // site asking to be left alone for a while, and being left alone. The
    // user still needs told, or an untouched tab is a mystery, but painting
    // it as a failure would send them looking for a fault that is not there.
    QString lastSweepNoticeText() const;

    // Sweeps every ticked site. Does nothing while a sweep is already running
    // — one at a time, so the numbers on screen always mean something.
    void startSweep();

    // --- Adding one job by hand ---
    //
    // This is the way around the sites Job Crush is not allowed to read. The
    // user pastes a link, or types the company and the job title straight off
    // a LinkedIn alert, and Job Crush goes and finds that same job on the
    // employer's own board — where it is allowed to look, and where the
    // posting is the real one instead of somebody's copy of it.
    //
    // When the job cannot be found, what the user typed is saved anyway. A
    // paste that appears to do nothing is worse than a plain no.
    //
    // One at a time, same as a sweep, so the status line always means one
    // thing.
    void addJobFromLink(const QString &pastedLink);
    void addJobFromCompanyAndTitle(const QString &companyName,
                                   const QString &positionTitle);

    // True while Job Crush is out looking for one pasted job.
    bool leadIsBeingResolved() const;

    // What happened, in one sentence for the user to read. Always ends with
    // something they can do next.
    QString leadStatusText() const;

    // Everything one site has delivered, newest first, each with its score.
    QList<ScoredJobPosting> scoredJobPostingsFromSource(
        const QString &sourceStorageName,
        SearchAreaScope searchAreaScope = SearchAreaScope::InsideSearchArea) const;

    // Every discovery from every site, best match first, each job appearing
    // once even when two sites carry it.
    QList<ScoredJobPosting> rankedTopProspects(
        SearchAreaScope searchAreaScope = SearchAreaScope::InsideSearchArea) const;

    // How many stored discoveries the location filter is holding back right
    // now. Shown to the user, always: a filter that quietly eats jobs and
    // never admits it is indistinguishable from a broken sweep.
    int jobPostingCountOutsideSearchArea() const;

    // True when the user has named any place at all — that is, when the
    // filter is doing anything.
    bool searchAreaIsNarrowed() const;

    // True once the search profile says enough for a ranking to mean
    // something. Asked through JobScout so nothing above has to reach past it
    // to the profile — the front door stays the only door.
    bool searchProfileCanRank() const;

signals:
    // sweepIsRunning() or sweepProgressText() changed.
    void sweepProgressChanged();

    // Stored discoveries changed — the lists above are worth re-reading.
    void discoveriesChanged();

    // leadIsBeingResolved() or leadStatusText() changed.
    void leadStatusChanged();

private:
    // The client for a site, or nullptr while that site's client has not been
    // written yet.
    JobSourceProvider *providerFor(const QString &sourceStorageName) const;

    // Wires up one site's reply and folds its results into the sweep.
    void beginSweepOfSource(const QString &sourceStorageName);

    // Goes looking for one lead on the employer's own board, then stores
    // whatever comes back — the real posting when it is found, and what the
    // user gave us when it is not.
    void goLookForOneLead(const JobLead &jobLead);

    // Saves one posting and returns the sentence to show the user. An empty
    // board name means the user handed this job over rather than a board
    // giving it up.
    QString storeOnePostingAndSayWhatHappened(JobPosting jobPosting,
                                              const QString &boardItCameFrom);

    // Saves the job as the user typed or pasted it, for when the real posting
    // could not be found, and says why in the same breath.
    QString keepWhatTheUserGaveUs(const JobLead &jobLead,
                                  const QString &whyTheRealOneIsMissing);

    void finishLeadWith(const QString &statusTextForTheUser);

    // Says something to the user WITHOUT ending the search that is running.
    // Used to turn down a second request while the first is still out.
    void sayThisAboutTheLead(const QString &statusTextForTheUser);

    // Stores one site's finds, counting what was genuinely new.
    void recordFindingsFromSource(const QString &sourceStorageName,
                                  const QList<JobPosting> &foundJobPostings);

    void finishSourceAndReportProgress(const QString &sourceStorageName,
                                       const QString &sourceOutcomeText,
                                       bool sourceHadTrouble);

    // Writes what every site said during the sweep that just ended to
    // lastSweep.log, replacing the previous one.
    //
    // Because a status line on a screen is a terrible place to keep the one
    // fact somebody needs later. When a site quietly stops working, the
    // question is always "what did it actually say?", and the honest answer
    // has to survive being scrolled past, alt-tabbed away from, or read at
    // two in the morning by someone who is tired.
    void writeSweepLog() const;

    const QString sweepLogFolderPath;

    JobPostingRepository &discoveredJobPostingRepository;
    JobSourceRoster &registeredSourceRoster;
    FollowedEmployerRoster &watchedEmployerRoster;
    JobSearchProfile &userSearchProfile;

    // A plain vector rather than a keyed container: QHash is implicitly
    // shared and so needs a copyable value, which a unique_ptr is not. No
    // loss — every provider already knows its own storage name, so looking
    // one up by asking is simpler than keeping a second list of keys in step
    // with this one, and there will only ever be a handful of them.
    std::vector<std::unique_ptr<JobSourceProvider>> builtJobSourceProviders;

    // Built once and kept, same as the site clients. Held by pointer so this
    // header does not have to drag in all three board readers.
    std::unique_ptr<CanonicalPostingResolver> employerBoardResolver;

    // Live bookkeeping for one pasted job.
    bool aLeadIsBeingResolved = false;
    QString storedLeadStatusText;

    // Live sweep bookkeeping. Cleared when a sweep ends.
    QStringList sourcesStillSweeping;
    QStringList finishedSourceOutcomeLines;
    QStringList sourcesThatHadTroubleThisSweep;
    QStringList sourceNoticesThisSweep;
    QString storedLastSweepTroubleText;
    QString storedLastSweepNoticeText;
    int jobPostingsRefusedThisSource = 0;
    QString firstRefusalReasonThisSource;
    int totalJobsFoundThisSweep = 0;
    int totalNewJobsThisSweep = 0;
    QString storedLastSweepSummaryText;
};
