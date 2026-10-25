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

// Which rows the Discoveries page wants.
struct DiscoveryRowRequest {
    enum class RowKind {
        TopProspects,       // every found job inside the search area, best first
        OutsideSearchArea,  // every found job the location filter holds back
        OneSource,          // one site's jobs inside the search area, newest first
        HandAdded           // the jobs the user added, the one added last first
    };

    RowKind rowKind = RowKind::TopProspects;
    QString sourceStorageName;   // used only for OneSource
};

// The rows for one request, and how many found jobs the location filter is
// holding back. The count is worked out on every request, because the tab
// row shows it whichever tab is open.
struct DiscoveryRows {
    QList<ScoredJobPosting> scoredJobPostings;
    int jobCountOutsideSearchArea = 0;
};

// What happened when one batch of found jobs was saved.
struct FindingsSaveOutcome {
    int jobsSaved = 0;          // saved now, or already known
    int newJobs = 0;            // saved now for the first time
    int jobsRefused = 0;        // the database would not take them
    QString firstRefusalReason; // the database's own words for the first one
};

// Two boards can carry the same job. This key is how Job Crush tells that
// two postings are the same job: the company and the title together,
// lowercase, with everything except letters and numbers taken out. That way
// a difference in spacing or punctuation between two boards does not make
// the same job show twice.
QString crossSourceIdentityOf(const JobPosting &jobPosting);

Q_DECLARE_METATYPE(DiscoveryRowRequest)
Q_DECLARE_METATYPE(DiscoveryRows)
Q_DECLARE_METATYPE(FindingsSaveOutcome)

// DiscoveryWorker
//
// The slow work behind the Discoveries page, done on a background thread so
// the window never freezes. It saves found jobs, marks jobs that are gone
// from a company's board, and loads and scores the rows for a tab.
//
// With thousands of found jobs, each of these can take a second or more.
// Done on the screen's thread, that froze the window, and Windows called Job
// Crush "not responding". Here, the window keeps drawing while the work goes
// on.
//
// It has its own connection to the database, because Qt lets a connection be
// used only on the thread that opened it. JobScout owns it and talks to it
// only through queued calls and signals. Nothing else touches it.
class DiscoveryWorker : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryWorker(const QString &databaseFilePath);
    ~DiscoveryWorker() override;

    // Opens this thread's database connection. Call it first, on the worker's
    // own thread.
    void openDatabase();

    // Saves one batch of found jobs, such as one company's answer. Jobs
    // already known are counted, not saved again. Sends findingsSaved.
    void saveFindings(const QString &sourceStorageName,
                      const QList<JobPosting> &foundJobPostings);

    // For sources that read a company's whole board: finds the saved jobs
    // from those companies that this sweep did not find. A job that was
    // marked gone and is listed again loses the mark here. The missing ones
    // are only sent back, not marked: one missing answer is not enough to
    // call a job gone, so JobScout checks each one again first. Jobs the
    // user said are still open are left out. Sends jobsMissingFromBoards.
    void findJobsMissingFromBoardsReadInFull(const QString &sourceStorageName,
                                             const QStringList &employersReadInFull,
                                             const QList<JobPosting> &foundJobPostings);

    // Loads and scores the rows for one tab. Sends rowsPrepared with the same
    // request number, so the caller can drop answers to old requests.
    void prepareRows(int requestNumber, const DiscoveryRowRequest &rowRequest,
                     const ScoringProfile &scoringProfile);

signals:
    void findingsSaved(const QString &sourceStorageName,
                       const FindingsSaveOutcome &saveOutcome);
    void jobsMissingFromBoards(const QString &sourceStorageName,
                               const QList<JobPosting> &missingJobPostings);
    void rowsPrepared(int requestNumber, const DiscoveryRows &preparedRows);

private:
    // Every found job, best match first, each job once even when two sites
    // carry it. insideSearchArea picks which side of the location filter.
    QList<ScoredJobPosting> rankedJobPostings(const QList<JobPosting> &allDiscoveredJobPostings,
                                              const ProspectScorer &prospectScorer,
                                              bool insideSearchArea) const;

    const QString databaseFilePath;
    std::unique_ptr<JobCrushDatabase> workerDatabase;
    std::unique_ptr<JobPostingRepository> workerJobPostingRepository;
    bool databaseIsOpen = false;
};
