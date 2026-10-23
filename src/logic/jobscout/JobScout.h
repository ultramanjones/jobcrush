#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <memory>
#include <vector>

#include "../../model/JobPosting.h"
#include "ProspectScorer.h"

class AiBrain;
class BrainJobFinder;
class PastedTextJobReader;
class CanonicalPostingResolver;
class FollowedEmployerRoster;
struct JobLead;
class JobPostingRepository;
class PostingSourceRepository;
class JobSearchProfile;
class JobSourceProvider;
class JobSourceRoster;
class PostedJobPageReader;

// ScoredJobPosting
//
// A posting and its score against the search profile, kept together. One
// list of these is simpler than two lists the caller must keep in the same
// order.
struct ScoredJobPosting {
    JobPosting jobPosting;
    ProspectMatchResult matchResult;
};

// SearchAreaScope
//
// Which side of the location filter a caller wants: the jobs where the user
// said they will work, or the jobs held back because they are somewhere else.
//
// OutsideSearchArea exists so the held-back jobs can still be looked at, on
// their own tab. Hiding jobs with no way to see them would be the app making
// the decision for the user.
enum class SearchAreaScope {
    InsideSearchArea,
    OutsideSearchArea
};

// JobScout
//
// Finds jobs. A Logic layer class. Everything above it asks JobScout for jobs
// and never touches a job site, an HTTP request, or the scoring directly.
//
// What it does:
//  - runs a sweep across the sites the user ticked, one reply per site,
//  - saves every job found through the repository, which knows which jobs
//    it has seen before,
//  - ranks the stored jobs against the search profile,
//  - reports progress with real numbers while it works,
//  - finds the employer's own posting for a job the user adds by hand.
//
// JobScout never creates a JobApplication. A found job goes on the board
// only when the user presses CRUSH. JobScout finds jobs; the user decides.
class JobScout : public QObject {
    Q_OBJECT
public:
    // diagnosticsFolderPath is where the sweep log is written — the app's own
    // data folder, handed in by the composition root so this class never has
    // to know where that is.
    JobScout(JobPostingRepository &jobPostingRepository,
             PostingSourceRepository &postingSourceRepository,
             JobSourceRoster &jobSourceRoster,
             FollowedEmployerRoster &followedEmployerRoster,
             JobSearchProfile &searchProfile,
             AiBrain &aiBrain,
             const QString &diagnosticsFolderPath,
             QObject *parent = nullptr);
    ~JobScout() override;

    // True while at least one site is still answering.
    bool sweepIsRunning() const;

    // What is happening, in real numbers, for the page to show in place of a
    // spinner: "Remotive: 43 found, 11 new · Arbeitnow: looking…"
    QString sweepProgressText() const;

    // How the last finished sweep went, kept on screen afterward so the user
    // can see what a run actually produced.
    QString lastSweepSummaryText() const;

    // Any site that could not answer, and what it said. This is kept after
    // the sweep ends, instead of being replaced by the summary, so the user
    // can still see what went wrong.
    QString lastSweepTroubleText() const;

    // A message from a site that is neither a count nor a fault. Most often a
    // site asked Job Crush to wait before asking again, and Job Crush is
    // waiting. The page shows it, but not as an error, because nothing is
    // broken.
    QString lastSweepNoticeText() const;

    // Sweeps every ticked site. Does nothing while a sweep is already running,
    // so the numbers on screen always belong to one sweep.
    void startSweep();

    // --- Adding one job by hand ---
    //
    // This is how the user adds a job from a site Job Crush is not allowed to
    // read. The user pastes a link, or types the company and the job title
    // off a LinkedIn alert. Job Crush then looks for the same job: on the
    // employer's own board first, then by reading the pasted page, then by
    // asking the AI brain to search the web, when one is connected.
    //
    // When the job cannot be found, what the user gave is saved anyway, so
    // the user's work is never lost.
    //
    // One job at a time, so the status line is always about one job.
    void addJobFromLink(const QString &pastedLink);
    void addJobFromCompanyAndTitle(const QString &companyName,
                                   const QString &positionTitle);

    // --- Pasted text, read by Moonlight (BACKLOG item 12) ---
    //
    // The user pastes an email, an alert or a posting. With a brain
    // connected, Moonlight fills in a form from it; the user checks the form
    // and adds the job, and then the usual hunt for the employer's own
    // posting runs. Anything the text did not say stays empty in the form.
    void readPastedText(const QString &pastedText);
    bool pastedTextIsBeingRead() const;
    QString pastedTextStatusText() const;
    bool aBrainCanReadPastedText() const;

    // What Moonlight filled in, or an empty posting when nothing is filled.
    JobPosting filledFormFromPastedText() const;
    void clearFilledForm();

    // Adds the job from the form the user checked. formFields carries the
    // boxes as the user left them.
    void addJobFromFilledForm(const JobPosting &formFields);

    // --- Every place a job can be applied to (BACKLOG item 13) ---
    //
    // Looks for the same job on the employer's own board (Greenhouse, Lever,
    // Ashby, SmartRecruiters, Workday) and, when it is there, ADDS that as
    // another place to apply. The job itself is never replaced. Runs when a
    // job is crushed onto the board, and when the user asks on a card.
    // Quiet: finding nothing is a normal answer and changes nothing.
    void lookForOtherPlacesToApply(qint64 jobPostingId);

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

    // Every job the user added by hand, newest added first.
    //
    // There is no search-area argument, on purpose. The location filter keeps
    // a sweep from filling the list with jobs in the wrong part of the
    // country. A job the user added by hand is one they asked for, so the
    // filter never hides it.
    QList<ScoredJobPosting> handAddedJobPostings() const;

    // The job the user added by hand most recently, so the page can go
    // straight to it. 0 when there is nothing to go to.
    qint64 mostRecentlyHandAddedJobPostingId() const;

    // How many stored discoveries the location filter is holding back right
    // now. The page always shows this number. Without it, a filter that hides
    // most jobs would look like a sweep that is broken.
    int jobPostingCountOutsideSearchArea() const;

    // True when the user has named any place at all — that is, when the
    // filter is doing anything.
    bool searchAreaIsNarrowed() const;

    // True once the search profile has enough in it to rank by. The
    // viewmodel asks JobScout rather than the profile, so JobScout stays the
    // one class it talks to for discovery.
    bool searchProfileCanRank() const;

signals:
    // sweepIsRunning() or sweepProgressText() changed.
    void sweepProgressChanged();

    // Stored discoveries changed — the lists above are worth re-reading.
    void discoveriesChanged();

    // leadIsBeingResolved() or leadStatusText() changed.
    void leadStatusChanged();

    // The pasted-text form changed: being read, filled, or cleared.
    void pastedTextFormChanged();

    // A new place to apply was found for this job.
    void placesToApplyChanged(qint64 jobPostingId);

private:
    // The client for a site, or nullptr while that site's client has not been
    // written yet.
    JobSourceProvider *providerFor(const QString &sourceStorageName) const;

    // Wires up one site's reply and folds its results into the sweep.
    void beginSweepOfSource(const QString &sourceStorageName);

    // The hunt for one hand-added job, one step per method. Each step either
    // saves the job or hands the lead to the next step. brainWasAlreadyAsked
    // stops the brain being asked twice for one job.
    void beginHuntForOneLead(const JobLead &jobLead);
    void huntTheEmployerBoards(const JobLead &jobLead, bool brainWasAlreadyAsked);
    void readThePostedPage(const JobLead &jobLead, bool brainWasAlreadyAsked);
    void whenTheHuntCameUpEmpty(const JobLead &jobLead, const QString &whyNothingWasFound,
                                bool brainWasAlreadyAsked);
    void askTheBrainToFindIt(const JobLead &jobLead, const QString &whyEarlierStepsMissed);

    // Saves the posting a step found and tells the user where it came from.
    void keepTheRealPosting(const JobLead &jobLead, const JobPosting &realPosting,
                            bool theBrainFoundTheLink, bool onlyThePageTitleWasRead);

    // Keeps one more place where the user can apply for a job they already
    // have. Safe to call with a place already recorded.
    void rememberRouteToThisJob(qint64 jobPostingId, const QString &boardName,
                                const QString &externalId, const QString &postingUrl);

    // Saves one hand-added posting and returns the sentence to show the
    // user. An empty boardItCameFrom means no board had the job, and the user
    // gave Job Crush the details.
    QString storeOnePostingAndSayWhatHappened(JobPosting jobPosting,
                                              const QString &boardItCameFrom);

    // Saves the job as the user typed or pasted it, for when the real posting
    // could not be found. The returned sentence also says why.
    QString keepWhatTheUserGaveUs(const JobLead &jobLead,
                                  const QString &whyTheRealOneIsMissing);

    void finishLeadWith(const QString &statusTextForTheUser);

    // Says something to the user WITHOUT ending the search that is running.
    // Used to turn down a second request while the first is still out.
    void sayThisAboutTheLead(const QString &statusTextForTheUser);

    // Marks the jobs that are gone from a company's board. Only for sources
    // that read a company's whole board, like the companies the user follows.
    // A job from one of those companies that the board no longer lists has
    // been taken down. A job that comes back loses the mark. Nothing is
    // deleted.
    void markJobsGoneFromBoardsReadInFull(const QString &sourceStorageName,
                                          const QStringList &employersReadInFull,
                                          const QList<JobPosting> &foundJobPostings);

    // Saves one site's finds, and counts how many were new.
    void recordFindingsFromSource(const QList<JobPosting> &foundJobPostings);

    void finishSourceAndReportProgress(const QString &sourceStorageName,
                                       const QString &sourceOutcomeText,
                                       bool sourceHadTrouble);

    // Writes what every site said during the sweep that just ended to
    // lastSweep.log, replacing the last one.
    //
    // The status line on the page is gone once the next sweep starts. When a
    // site stops working, the first question is what it said, and this file
    // keeps the answer.
    void writeSweepLog() const;

    const QString sweepLogFolderPath;

    JobPostingRepository &jobPostingRepository;
    PostingSourceRepository &postingSourceRepository;
    JobSourceRoster &jobSourceRoster;
    FollowedEmployerRoster &followedEmployerRoster;
    JobSearchProfile &searchProfile;

    // A plain vector, not a QHash, because QHash needs values it can copy and
    // a unique_ptr cannot be copied. Every provider knows its own storage
    // name, so providerFor() finds one by asking each. There are only a
    // handful.
    std::vector<std::unique_ptr<JobSourceProvider>> builtJobSourceProviders;

    // Built once and kept, the same as the site clients. Held by pointer so
    // this header does not have to include every board reader.
    std::unique_ptr<CanonicalPostingResolver> employerBoardResolver;

    // Reads a job off a pasted web page. Same lifetime as the resolver.
    std::unique_ptr<PostedJobPageReader> postedPageReader;

    // Asks the connected brain to search the web. Same lifetime.
    std::unique_ptr<BrainJobFinder> brainJobFinder;

    // Moonlight reading pasted text into a form, and the form so far.
    std::unique_ptr<PastedTextJobReader> pastedTextReader;
    bool pastedTextBeingRead = false;
    QString storedPastedTextStatus;
    JobPosting storedFilledForm;

    // Live bookkeeping for one hand-added job.
    bool aLeadIsBeingResolved = false;
    QString storedLeadStatusText;

    // The job the user added by hand most recently, so the page can jump
    // straight to it. 0 means there is nowhere to jump.
    qint64 lastHandAddedJobPostingId = 0;

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
