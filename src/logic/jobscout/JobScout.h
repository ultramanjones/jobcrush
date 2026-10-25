#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>

#include <memory>
#include <vector>

#include "../../model/JobPosting.h"
#include "../pipelines/PostingGoneConfirmer.h"
#include "DiscoveryWorker.h"
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

// JobScout
//
// Finds jobs. A Logic layer class. Everything above it asks JobScout for jobs
// and never touches a job site, an HTTP request, or the scoring directly.
//
// What it does:
//  - runs a sweep across the sites the user ticked, one reply per site,
//  - saves every job found, one company at a time, on a background thread,
//  - prepares the ranked rows for each Discoveries tab, also on that thread,
//  - reports progress with real numbers while it works,
//  - finds the employer's own posting for a job the user adds by hand.
//
// JobScout never creates a JobApplication. A found job goes on the board
// only when the user presses CRUSH. JobScout finds jobs; the user decides.
class JobScout : public QObject {
    Q_OBJECT
public:
    // diagnosticsFolderPath is where the sweep log is written: the app's own
    // data folder. databaseFilePath is the database file, which the
    // background thread opens a second connection to. Both are handed in by
    // the composition root, so this class never has to work them out.
    JobScout(JobPostingRepository &jobPostingRepository,
             PostingSourceRepository &postingSourceRepository,
             JobSourceRoster &jobSourceRoster,
             FollowedEmployerRoster &followedEmployerRoster,
             JobSearchProfile &searchProfile,
             AiBrain &aiBrain,
             const QString &diagnosticsFolderPath,
             const QString &databaseFilePath,
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
    // Ashby, SmartRecruiters, Workday, Taleo) and, when it is there, ADDS that as
    // another place to apply. The job itself is never replaced. Runs when a
    // job is crushed onto the board, and when the user asks on a card.
    // Quiet: finding nothing is a normal answer and changes nothing.
    void lookForOtherPlacesToApply(qint64 jobPostingId);

    // True while Job Crush is out looking for one pasted job.
    bool leadIsBeingResolved() const;

    // What happened, in one sentence for the user to read. Always ends with
    // something they can do next.
    QString leadStatusText() const;

    // Asks for the rows of one Discoveries tab. They are loaded and scored on
    // the background thread, and arrive in rowsReady with the number this
    // returns. A newer request makes the older ones stale, and the caller
    // should drop their answers.
    //
    // The Manual Add rows ignore the location filter, on purpose. A job the
    // user added by hand is one they asked for, so the filter never hides it.
    int requestRows(const DiscoveryRowRequest &rowRequest);

    // The job the user added by hand most recently, so the page can go
    // straight to it. 0 when there is nothing to go to.
    qint64 mostRecentlyHandAddedJobPostingId() const;

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

    // Stored discoveries changed, so the rows are worth asking for again.
    // During a sweep this is sent at most about once a second, however many
    // companies answer, so the page is not rebuilt hundreds of times.
    void discoveriesChanged();

    // The rows asked for with requestRows. The count of jobs the location
    // filter holds back comes with every answer.
    void rowsReady(int requestNumber, const DiscoveryRows &preparedRows);

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

    // What one site's part of the sweep has done so far. A site is finished
    // when its reply has come back AND the background thread has saved
    // everything it sent.
    struct SourceSweepProgress {
        QString sourceDisplayName;
        bool replyHasFinished = false;
        bool replyFailed = false;
        QString failureReason;
        bool failureIsTrouble = true;
        int jobsArrived = 0;
        int backgroundTasksPending = 0;
        FindingsSaveOutcome saveTotals;
    };

    // Hands one batch of found jobs to the background thread to save.
    void saveFindingsInTheBackground(const QString &sourceStorageName,
                                     const QList<JobPosting> &foundJobPostings);

    // The background thread finished saving one batch.
    void whenFindingsWereSaved(const QString &sourceStorageName,
                               const FindingsSaveOutcome &saveOutcome);

    // The background thread found saved jobs that a whole-board read did
    // not find. Each one is checked again on the company's board, and marked
    // gone only when that check agrees.
    void whenJobsAreMissingFromBoards(const QString &sourceStorageName,
                                      const QList<JobPosting> &missingJobPostings);

    // Finishes a site's part of the sweep, once its reply is back and all its
    // background work is done. Does nothing before then.
    void finishSourceIfEverythingIsDone(const QString &sourceStorageName);

    void finishSourceAndReportProgress(const QString &sourceStorageName,
                                       const QString &sourceOutcomeText,
                                       bool sourceHadTrouble);

    // Sends discoveriesChanged soon, at most about once a second, however
    // often this is called.
    void sayDiscoveriesChangedSoon();

    // Writes what every site said during the sweep that has ended to
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

    // The background thread, and the worker that lives on it. The worker is
    // used only through queued calls, never directly from this thread.
    QThread discoveryThread;
    DiscoveryWorker *discoveryWorker = nullptr;
    int lastRowRequestNumber = 0;

    // Checks a job that went missing, before Job Crush calls it gone.
    PostingGoneConfirmer goneConfirmer;

    // Holds discoveriesChanged back during a sweep. See sayDiscoveriesChangedSoon.
    QTimer discoveriesChangedTimer;

    // Live sweep bookkeeping. Cleared when a sweep ends.
    QHash<QString, SourceSweepProgress> sourceSweepProgress;
    QStringList sourcesStillSweeping;
    QStringList finishedSourceOutcomeLines;
    QStringList sourcesThatHadTroubleThisSweep;
    QStringList sourceNoticesThisSweep;
    QString storedLastSweepTroubleText;
    QString storedLastSweepNoticeText;
    int totalJobsFoundThisSweep = 0;
    int totalNewJobsThisSweep = 0;
    QString storedLastSweepSummaryText;
};
