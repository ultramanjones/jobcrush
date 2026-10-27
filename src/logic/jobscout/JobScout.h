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
// Finds jobs. JobScout is a Logic layer class. The view models ask JobScout
// for jobs. They never call a job site, send an HTTP request, or score a
// job themselves.
//
// What JobScout does:
//  - It runs a sweep. A sweep asks every site the user ticked in Settings
//    for jobs. Each site answers with one JobScoutReply.
//  - It saves every job found, on a background thread. Some sites hear from
//    many companies. Their jobs are saved one company at a time.
//  - It loads and scores the rows for each tab of the Discoveries page, on
//    that same background thread.
//  - It reports progress with real counts while the sweep runs.
//  - When the user adds a job by hand, it looks for the employer's own
//    posting of that job.
//
// JobScout never creates a JobApplication. A found job goes on the Job
// Pipelines board only when the user presses CRUSH. JobScout only finds
// jobs. The user decides which ones to go after.
class JobScout : public QObject {
    Q_OBJECT
public:
    // diagnosticsFolderPath is the folder where writeSweepLog() writes
    // lastSweep.log. It is the app's own data folder. databaseFilePath is
    // the path to the database file. The background thread opens its own,
    // second connection to that file. The code that builds the app's objects
    // at startup passes in both paths, so this class does not look them up.
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

    // True while a sweep is running. A sweep is running while at least one
    // site has not sent its answer, or its jobs are not all saved yet.
    bool sweepIsRunning() const;

    // One line of text that tells the user how the sweep is going, with real
    // counts. The Discoveries page shows it instead of a spinner. Each site
    // gets its own piece, for example:
    // "Remotive: 43 found, 11 new   ·   Arbeitnow: looking…"
    QString sweepProgressText() const;

    // A one-line summary of the last sweep that finished, such as
    // "120 jobs checked · 14 new · 3 sites". It stays on screen after the
    // sweep ends, so the user can see what the sweep found.
    QString lastSweepSummaryText() const;

    // One piece of text for each site that had a problem in the last sweep,
    // and what went wrong. A problem is a failed request, a site that sent
    // no jobs, or jobs that Job Crush could not save. This text stays on
    // screen after the sweep ends. The summary does not replace it, so the
    // user can still see what went wrong.
    QString lastSweepTroubleText() const;

    // Messages from sites that failed in a way that is not a problem. Most
    // often a site asked Job Crush to wait before asking again, and Job Crush
    // is waiting. The page shows these messages, but not as errors, because
    // nothing is broken.
    QString lastSweepNoticeText() const;

    // Starts a sweep of every site the user ticked in Settings. If a sweep
    // is already running, this does nothing. That way the counts on screen
    // always come from one sweep, not two mixed together.
    void startSweep();

    // --- Adding one job by hand ---
    //
    // The user can add a job by hand. This is how the user adds a job from a
    // site Job Crush is not allowed to read, such as LinkedIn. The user
    // pastes a link, or types the company and the job title from a LinkedIn
    // alert. Job Crush then looks for the same job:
    //  1. If the link points to a known job board, or the user typed a
    //     company and a title, Job Crush searches the employer's own job
    //     board. Otherwise it reads the web page at the pasted link.
    //  2. If that finds nothing, and the connected AI brain can search the
    //     web, Job Crush asks the brain to search the web for the job.
    //
    // If the job cannot be found, Job Crush saves what the user gave anyway,
    // so the user's work is not lost. If the user gave only a link and Job
    // Crush could not read it, there is nothing to save.
    //
    // Only one job is looked up at a time, so the status line always talks
    // about one job.
    void addJobFromLink(const QString &pastedLink);
    void addJobFromCompanyAndTitle(const QString &companyName,
                                   const QString &positionTitle);

    // --- Pasted text, read by Moonlight (BACKLOG item 12) ---
    //
    // Moonlight is the app's AI assistant. It works through whichever AI
    // brain the user connected. The user pastes an email, a job alert or a
    // job posting. If a brain is connected, Moonlight reads the text and
    // fills in a form with the job's details. The user checks the form and
    // presses Add this job. Then Job Crush looks for the employer's own
    // posting, the same way it does for any job added by hand. If the text
    // did not mention a detail, that box in the form stays empty.
    void readPastedText(const QString &pastedText);
    bool pastedTextIsBeingRead() const;
    QString pastedTextStatusText() const;
    bool aBrainCanReadPastedText() const;

    // The form Moonlight filled in, as a JobPosting. It is an empty
    // JobPosting if nothing has been filled in.
    JobPosting filledFormFromPastedText() const;
    void clearFilledForm();

    // Adds the job from the form, after the user has checked it. formFields
    // holds what is in each box of the form, including any changes the user
    // made. The form must have at least a company and a job title.
    void addJobFromFilledForm(const JobPosting &formFields);

    // --- Every place a job can be applied to (BACKLOG item 13) ---

    // How a search for another place to apply ended.
    enum class OtherPlaceSearchResult {
        FoundAnotherPlace,              // a new place to apply is on the card
        NothingElseFound,               // it searched and found nothing new
        AlreadyOnTheEmployersOwnBoard,  // the saved link is already the best
        NothingToSearchWith,            // no company name or no job title
        JobIsNotInTheDatabase,          // no job has that id
    };
    Q_ENUM(OtherPlaceSearchResult)

    // A job can often be applied to in more than one place. This function
    // first records the place where the job was found. Then it looks for the
    // same job on the employer's own job board (Greenhouse, Lever, Ashby,
    // SmartRecruiters, Workday or Taleo). If the job is there, it ADDS that
    // board as one more place to apply. It never replaces the saved job.
    // It runs when the user presses CRUSH to put a job on the board, and
    // when the user asks for it on a job card. Every ending is reported with
    // lookingElsewhereFinished, including finding nothing. The user pressed a
    // button, so the user gets an answer.
    //
    // When another place is found, the "gone" mark is cleared. Job Crush just
    // read the job off a live board, so it is not gone.
    void lookForOtherPlacesToApply(qint64 jobPostingId);

    // True while Job Crush is looking for a job the user added by hand.
    bool leadIsBeingResolved() const;

    // The status line for the job the user added by hand, written for the
    // user to read. While the search runs, it says which step is running.
    // When the search ends, it says what happened, and when something went
    // wrong, what the user can do next.
    QString leadStatusText() const;

    // Asks for the rows of one tab on the Discoveries page. The background
    // thread loads and scores the rows. They come back in the rowsReady
    // signal, with the request number this function returns. Each call gets
    // a higher number than the last. If an answer's number is lower than the
    // caller's latest request, that answer is out of date, and the caller
    // should drop it.
    //
    // The Manual Add tab ignores the location filter on purpose. The user
    // added those jobs by hand, so the user wants them, and the filter never
    // hides them.
    int requestRows(const DiscoveryRowRequest &rowRequest);

    // The id of the job the user added by hand most recently. The
    // Discoveries page uses it to jump straight to that job's row. It is 0
    // when there is no such job, or while a new job is still being looked up.
    qint64 mostRecentlyHandAddedJobPostingId() const;

    // True when the user has listed at least one preferred work location in
    // the search profile. Only then does the location filter hide any jobs.
    bool searchAreaIsNarrowed() const;

    // True when the search profile has enough in it to rank jobs by. The
    // view model asks JobScout instead of asking the profile. That way
    // JobScout is the only class the view model calls about found jobs.
    bool searchProfileCanRank() const;

signals:
    // sweepIsRunning() or sweepProgressText() changed.
    void sweepProgressChanged();

    // The saved jobs changed, so the page should ask for its rows again.
    // During a sweep, this signal is sent at most once a second, no matter
    // how many companies answer. Otherwise the page would rebuild its rows
    // hundreds of times.
    void discoveriesChanged();

    // The rows asked for by a call to requestRows. requestNumber is the
    // number that call returned. preparedRows also holds a count of the
    // found jobs that the location filter hides, because they are outside
    // the user's search area.
    void rowsReady(int requestNumber, const DiscoveryRows &preparedRows);

    // leadIsBeingResolved() or leadStatusText() changed.
    void leadStatusChanged();

    // The pasted-text form or its status line changed. Moonlight started
    // reading the text, Moonlight filled in the form, or the form was
    // cleared.
    void pastedTextFormChanged();

    // A new place to apply was found for this job.
    void placesToApplyChanged(qint64 jobPostingId);

    // A search for another place to apply ended. Sent every time, however it
    // ended, so the screen can always say what happened. boardDisplayName
    // holds the board that was found and is empty for the endings that found
    // nothing.
    void lookingElsewhereFinished(qint64 jobPostingId,
                                  JobScout::OtherPlaceSearchResult result,
                                  const QString &boardDisplayName);

private:
    // Returns the client object that talks to the site with this storage
    // name. Returns nullptr if no client class has been written for that
    // site yet.
    JobSourceProvider *providerFor(const QString &sourceStorageName) const;

    // Asks one site for jobs and connects to the reply the site sends back.
    // The jobs in the reply are saved, and the site's counts are added to
    // the sweep's totals.
    void beginSweepOfSource(const QString &sourceStorageName);

    // The search for one job the user added by hand. Each function below is
    // one step of the search. A JobLead holds what the user gave: a link, or
    // a company and a job title. Each step either saves the job or passes
    // the JobLead on to the next step. brainWasAlreadyAsked is true after
    // the AI brain has been asked once. It keeps the brain from being asked
    // twice for the same job.
    void beginHuntForOneLead(const JobLead &jobLead);
    void huntTheEmployerBoards(const JobLead &jobLead, bool brainWasAlreadyAsked);
    void readThePostedPage(const JobLead &jobLead, bool brainWasAlreadyAsked);
    void whenTheHuntCameUpEmpty(const JobLead &jobLead, const QString &whyNothingWasFound,
                                bool brainWasAlreadyAsked);
    void askTheBrainToFindIt(const JobLead &jobLead, const QString &whyEarlierStepsMissed);

    // Saves the posting a step found and tells the user where it came from.
    void keepTheRealPosting(const JobLead &jobLead, const JobPosting &realPosting,
                            bool theBrainFoundTheLink, bool onlyThePageTitleWasRead);

    // Records one more place where the user can apply for a job that is
    // already saved. If that place is already recorded, nothing changes, so
    // it is safe to call more than once.
    void rememberRouteToThisJob(qint64 jobPostingId, const QString &boardName,
                                const QString &externalId, const QString &postingUrl);

    // Saves one job the user added by hand. Returns the sentence to show the
    // user. boardItCameFrom is the name of the job board the posting was
    // read from. If it is empty, the posting did not come from a known
    // board, and the sentence does not name one.
    QString storeOnePostingAndSayWhatHappened(JobPosting jobPosting,
                                              const QString &boardItCameFrom);

    // Saves the job with the details the user typed or pasted. This is used
    // when Job Crush could not find the employer's posting. Returns the
    // sentence to show the user, which also says why the posting was not
    // found. If the company or the title is missing, nothing is saved.
    QString keepWhatTheUserGaveUs(const JobLead &jobLead,
                                  const QString &whyTheRealOneIsMissing);

    void finishLeadWith(const QString &statusTextForTheUser);

    // Sets the status line for the hand-added job WITHOUT ending the search
    // that is running. It shows each step of the search. It is also used to
    // turn down a second request while the first search is still running.
    void sayThisAboutTheLead(const QString &statusTextForTheUser);

    // Progress for one site during a sweep. A site is finished only when two
    // things are true: its reply has come back, AND the background thread
    // has finished every task for that site. Those tasks are saving its jobs
    // and looking for saved jobs that are gone.
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

    // Sends one batch of found jobs to the background thread to be saved. A
    // batch is one company's jobs, or all of a site's jobs at once.
    void saveFindingsInTheBackground(const QString &sourceStorageName,
                                     const QList<JobPosting> &foundJobPostings);

    // Runs when the background thread has finished saving one batch. Adds
    // the batch's counts to the site's totals.
    void whenFindingsWereSaved(const QString &sourceStorageName,
                               const FindingsSaveOutcome &saveOutcome);

    // Runs when the background thread reports missing jobs. Some sites read
    // a company's whole job board. A missing job is a saved job from that
    // company that was not on the board this time. Each missing job is
    // checked again on the company's board. Job Crush marks it gone only if
    // that second check also says it is gone.
    void whenJobsAreMissingFromBoards(const QString &sourceStorageName,
                                      const QList<JobPosting> &missingJobPostings);

    // Marks one site as finished in the sweep, but only if its reply is back
    // and the background thread has finished all of that site's tasks. If
    // not, it does nothing.
    void finishSourceIfEverythingIsDone(const QString &sourceStorageName);

    void finishSourceAndReportProgress(const QString &sourceStorageName,
                                       const QString &sourceOutcomeText,
                                       bool sourceHadTrouble);

    // Sends discoveriesChanged one second from now, unless a send is already
    // waiting. So no matter how often this is called, the signal goes out at
    // most once a second.
    void sayDiscoveriesChangedSoon();

    // When a sweep ends, writes what each site said to the file
    // lastSweep.log. The file goes in sweepLogFolderPath, the folder the
    // constructor got as diagnosticsFolderPath. It replaces the file from
    // the sweep before.
    //
    // The status line on the page is cleared when the next sweep starts. If
    // a site stops working, the first thing to check is what that site said.
    // This file keeps that record.
    void writeSweepLog() const;

    const QString sweepLogFolderPath;

    JobPostingRepository &jobPostingRepository;
    PostingSourceRepository &postingSourceRepository;
    JobSourceRoster &jobSourceRoster;
    FollowedEmployerRoster &followedEmployerRoster;
    JobSearchProfile &searchProfile;

    // The clients for the job sites, one per site. This is a std::vector,
    // not a QHash, because QHash needs values it can copy, and a unique_ptr
    // cannot be copied. Each client knows its own storage name.
    // providerFor() finds a client by checking each one in turn. There are
    // only five, so that is fast.
    std::vector<std::unique_ptr<JobSourceProvider>> builtJobSourceProviders;

    // Looks for a job on the employer's own job board. It is built once in
    // the constructor and kept, like the site clients. It is held by
    // pointer, so this header does not have to include every board reader.
    std::unique_ptr<CanonicalPostingResolver> employerBoardResolver;

    // Reads a job from the web page at a pasted link. Built once in the
    // constructor and kept.
    std::unique_ptr<PostedJobPageReader> postedPageReader;

    // Asks the connected AI brain to search the web for a job. Built once in
    // the constructor and kept.
    std::unique_ptr<BrainJobFinder> brainJobFinder;

    // pastedTextReader has Moonlight read pasted text and fill in a form.
    // The next three fields hold whether that reading is running, its status
    // line, and the form as filled in so far.
    std::unique_ptr<PastedTextJobReader> pastedTextReader;
    bool pastedTextBeingRead = false;
    QString storedPastedTextStatus;
    JobPosting storedFilledForm;

    // The state of the search for a job the user added by hand: whether the
    // search is running, and its status line.
    bool aLeadIsBeingResolved = false;
    QString storedLeadStatusText;

    // The id of the job the user added by hand most recently, so the page
    // can jump straight to its row. 0 means there is no such job.
    qint64 lastHandAddedJobPostingId = 0;

    // The background thread, and the DiscoveryWorker that runs on it. This
    // class calls the worker only through queued calls, which run on the
    // background thread. It never calls the worker directly from the main
    // thread. lastRowRequestNumber is the number given to the most recent
    // requestRows call.
    QThread discoveryThread;
    DiscoveryWorker *discoveryWorker = nullptr;
    int lastRowRequestNumber = 0;

    // Checks a missing job again on the company's board, before Job Crush
    // marks it gone.
    PostingGoneConfirmer goneConfirmer;

    // A one-second, single-shot timer. During a sweep, it delays
    // discoveriesChanged so the signal goes out at most once a second. See
    // sayDiscoveriesChangedSoon.
    QTimer discoveriesChangedTimer;

    // The state of the sweep. startSweep() clears most of these when a new
    // sweep begins. sourcesStillSweeping empties out as each site finishes.
    // The three storedLastSweep... texts keep the results of the last sweep
    // on screen after it ends.
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
