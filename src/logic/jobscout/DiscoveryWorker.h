#pragma once

#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

#include "../../model/JobPosting.h"
#include "JobSearchProfile.h"
#include "ProspectScorer.h"

class JobCrushDatabase;
class JobPostingRepository;

// A request from the Discoveries page for the job rows of one tab. rowKind
// says which tab.
//
// Several tabs use the location filter. If the user named one or more places
// in the search profile, the filter hides a job unless the job is remote,
// names no location, or names one of those places.
// ProspectScorer::jobPostingIsInsideSearchArea makes that choice.
struct DiscoveryRowRequest {
    enum class RowKind {
        // Top Prospects tab: every saved job from every site that passes the
        // location filter, highest score first. A job that two sites list
        // shows once.
        TopProspects,
        // Outside your area tab: every saved job that the location filter
        // hides, highest score first. A job that two sites list shows once.
        OutsideSearchArea,
        // One site's tab: the jobs from the site named in sourceStorageName
        // that pass the location filter, newest posting first.
        OneSource,
        // Manual Add tab: the jobs the user added by hand, the most recently
        // added first. The location filter does not apply here.
        HandAdded
    };

    RowKind rowKind = RowKind::TopProspects;
    QString sourceStorageName;   // which site; used only for OneSource
};

// The answer to one DiscoveryRowRequest. scoredJobPostings holds the rows
// for the tab. jobCountOutsideSearchArea counts the saved jobs that the
// location filter hides. It counts every copy, so a job that two sites list
// counts twice. The "Outside your area (N)" tab label shows this count no
// matter which tab is open, so the worker counts it on every request. If the
// user named no places, nothing is hidden and the count is 0.
struct DiscoveryRows {
    QList<ScoredJobPosting> scoredJobPostings;
    int jobCountOutsideSearchArea = 0;
};

// The result of one saveFindings call: how many jobs were saved, how many
// were new, and how many the database failed to save.
struct FindingsSaveOutcome {
    int jobsSaved = 0;          // new jobs plus jobs that were saved before
    int newJobs = 0;            // jobs saved for the first time by this call
    int jobsRefused = 0;        // jobs the database failed to save
    QString firstRefusalReason; // the database error text for the first failure
};

// Returns a key that tells whether two postings are the same job. Two job
// sites can list the same job. The key is the company name plus the job
// title, in lowercase, with every character that is not a letter or a number
// removed. So if two sites write the same company and title with different
// spaces or punctuation, the job still shows only once.
QString crossSourceIdentityOf(const JobPosting &jobPosting);

Q_DECLARE_METATYPE(DiscoveryRowRequest)
Q_DECLARE_METATYPE(DiscoveryRows)
Q_DECLARE_METATYPE(FindingsSaveOutcome)

// DiscoveryWorker
//
// DiscoveryWorker does the slow database work for the Discoveries page on a
// background thread. It has three tasks:
//  - It saves the jobs that a sweep finds. A sweep is one round of asking
//    the job sites for jobs.
//  - It finds saved jobs that a company's job board no longer lists.
//  - It loads and scores the job rows for one tab.
//
// With thousands of saved jobs, each task can take a second or more. When
// this work ran on the screen's thread, the window froze, and Windows said
// Job Crush was "not responding". On a background thread, the window keeps
// drawing while the work runs.
//
// The worker opens its own connection to the database, because Qt lets a
// database connection be used only on the thread that opened it. JobScout
// creates the worker and moves it to the background thread. JobScout calls
// it only through queued calls, and gets the results back through the
// worker's signals. No other class calls it.
class DiscoveryWorker : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryWorker(const QString &databaseFilePath);
    ~DiscoveryWorker() override;

    // Opens the worker's own database connection. Call this first, on the
    // background thread. If it fails, the other functions send back empty
    // results.
    void openDatabase();

    // Saves a list of jobs that one site found, or that one company's board
    // listed. A job that is already saved is counted, but it is not saved a
    // second time. When done, it sends findingsSaved with the counts.
    void saveFindings(const QString &sourceStorageName,
                      const QList<JobPosting> &foundJobPostings);

    // Finds saved jobs that a company's job board no longer lists. Only the
    // "Companies you follow" source reads a company's whole board, so only
    // its sweeps call this. employersReadInFull lists the boards it read in
    // full, as "board/tenant" keys such as "greenhouse/acme".
    // foundJobPostings is every job it found in this sweep.
    //
    // It looks at each saved job from sourceStorageName that came from one
    // of those boards:
    //  - If the board lists the job again and the job was marked gone, this
    //    function clears the gone mark. Employers do repost jobs.
    //  - If the board does not list the job, the job is not marked gone, and
    //    the user has not said the job is still open, the job goes in the
    //    missing list.
    //
    // This function does not mark the missing jobs gone. One missing answer
    // is not enough proof. It sends the missing list with
    // jobsMissingFromBoards, and JobScout checks each job again before it
    // marks the job gone.
    void findJobsMissingFromBoardsReadInFull(const QString &sourceStorageName,
                                             const QStringList &employersReadInFull,
                                             const QList<JobPosting> &foundJobPostings);

    // Loads and scores the job rows for the tab that rowRequest names.
    // scoringProfile is a copy of the user's search profile. When done, it
    // sends rowsPrepared with the same requestNumber. The user can switch
    // tabs before an answer comes back, so the caller ignores any answer
    // whose number is not the newest request.
    void prepareRows(int requestNumber, const DiscoveryRowRequest &rowRequest,
                     const ScoringProfile &scoringProfile);

signals:
    void findingsSaved(const QString &sourceStorageName,
                       const FindingsSaveOutcome &saveOutcome);
    void jobsMissingFromBoards(const QString &sourceStorageName,
                               const QList<JobPosting> &missingJobPostings);
    void rowsPrepared(int requestNumber, const DiscoveryRows &preparedRows);

private:
    // Scores the jobs in allDiscoveredJobPostings and sorts them, highest
    // score first. If two sites list the same job, only the copy with the
    // higher score is kept. If insideSearchArea is true, it keeps only the
    // jobs that pass the location filter. If it is false, it keeps only the
    // jobs that the filter hides.
    QList<ScoredJobPosting> rankedJobPostings(const QList<JobPosting> &allDiscoveredJobPostings,
                                              const ProspectScorer &prospectScorer,
                                              bool insideSearchArea) const;

    const QString databaseFilePath;
    std::unique_ptr<JobCrushDatabase> workerDatabase;
    std::unique_ptr<JobPostingRepository> workerJobPostingRepository;
    bool databaseIsOpen = false;
};
