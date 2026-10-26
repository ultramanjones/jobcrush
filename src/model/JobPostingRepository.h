#pragma once

#include <QList>
#include <QString>

#include "JobPosting.h"

class JobCrushDatabase;
class QSqlQuery;

// JobPostingRepository
//
// Reads and writes rows of the jobPosting table in the database. Each row
// is one job. The rest of the app works with JobPosting objects and never
// writes SQL for this table. Only this class writes that SQL.
//
// Every method that fails logs what went wrong, which method it was, and
// the job it was working on. lastErrorText() gives the same reason to the
// caller.
class JobPostingRepository {
public:
    explicit JobPostingRepository(JobCrushDatabase &database);

    // Saves jobPosting as a new row. On success, it sets
    // jobPosting.jobPostingId to the id of the new row and returns true.
    // If the database fails, it returns false.
    bool insertJobPosting(JobPosting &jobPosting);

    // Saves a job that JobScout found on a job board or site. A sweep is one
    // JobScout run over all the sources. A source is one board or site.
    //
    // If the same source already sent this same job (same postingSource and
    // same externalSourceId), this does not save it again. It sets
    // jobPosting.jobPostingId to the id of the row already stored, and sets
    // wasAlreadyKnown to true. If the job is new, it saves it and sets
    // wasAlreadyKnown to false. A sweep uses wasAlreadyKnown to report counts
    // such as "14 new, 61 already seen". If externalSourceId is empty, the
    // job is always saved as new.
    //
    // Returns false only when the database fails. A job seen before is a
    // normal result, not an error.
    bool insertDiscoveryIfNew(JobPosting &jobPosting, bool &wasAlreadyKnown);

    // Returns every job posting in the database. The job Job Crush found or
    // added most recently comes first (sorted by discoveredTimestamp).
    QList<JobPosting> loadAllJobPostings();

    // Returns every job posting from one board or site, named by
    // postingSource. It is sorted newest first by the date the employer
    // posted the job. If a job has no posted date, the date Job Crush found
    // it is used instead. The Discoveries page has one tab per source, and
    // this list fills the tab for this source.
    QList<JobPosting> loadJobPostingsFromSource(const QString &postingSource);

    // Sets scoutSource to "you" on a job that is already stored. "you" means
    // the user added the job by hand.
    //
    // A sweep often finds a job before the user goes looking for it. Later,
    // the user may paste the same job in by hand. Job Crush then tells the
    // user to look for it on the Manual Add tab. That tab shows only jobs
    // with scoutSource "you". So this code changes the job's scoutSource
    // from "scout" to "you". If it did not, the job would not show on the
    // Manual Add tab.
    bool markJobPostingAsHandAdded(qint64 jobPostingId);

    // Changes six fields on the row whose id is jobPosting.jobPostingId:
    // company, title, location, pay, link, and whether the job is remote.
    // It does not change any other column. Each job on the board has its own
    // folder with a JobQuest.txt file. Job Crush calls this method when an
    // outside AI fixes one of these six fields in that file.
    bool updateJobPostingDetails(const JobPosting &jobPosting);

    // Saves the time when Job Crush found that this job was no longer listed
    // on the employer's board. This is the "gone" mark. If goneSinceTimestamp
    // is invalid, this clears the "gone" mark instead. That is for a job that
    // is listed on the board again. If the user has said the job is still
    // open, this never sets the mark, but it can still clear it.
    bool updatePostingGoneSince(qint64 jobPostingId, const QDateTime &goneSinceTimestamp);

    // Saves the current time as the time the user said this job is still
    // open. It also clears any "gone" mark on the job. After this,
    // updatePostingGoneSince will never mark the job gone.
    bool markStillOpenByUser(qint64 jobPostingId);

    // Returns every job with scoutSource "you", which means the user added
    // it by hand. This list fills the Manual Add tab. It is sorted newest
    // first by discoveredTimestamp, the time Job Crush first stored the job.
    // It is not sorted by the date the employer posted the job. For a job
    // the user typed or pasted in, discoveredTimestamp is the time the user
    // added it. For a job a sweep found first and the user pasted in later,
    // discoveredTimestamp is still the time of the sweep.
    QList<JobPosting> loadHandAddedJobPostings();

    // Returns every job posting that has an id from its source, meaning
    // externalSourceId is not empty. These are the jobs JobScout found, from
    // all sources together. A job JobScout found with no id is left out. The
    // list is sorted newest first by the date the employer posted the job,
    // or by the date Job Crush found it if there is no posted date. The Top
    // Prospects page ranks this whole list, not one source's list.
    QList<JobPosting> loadAllDiscoveredJobPostings();

    // Returns the job posting with this id. If no row has that id, it sets
    // jobPostingWasFound to false and returns an empty JobPosting.
    JobPosting loadJobPostingById(qint64 jobPostingId, bool &jobPostingWasFound);

    // Returns the error text from the last insert, update or query that
    // failed.
    QString lastErrorText() const;

    // beginManyChanges starts a database transaction. finishManyChanges
    // commits it. Without a transaction, SQLite writes to the disk once for
    // each job saved. With thousands of jobs, that froze the window. To save
    // many jobs, call beginManyChanges, make the changes, then call
    // finishManyChanges. The disk is then written once for all of them.
    bool beginManyChanges();
    bool finishManyChanges();

private:
    // Runs a query that is already prepared and has its values bound.
    // Returns true if the query ran. If it fails, this calls recordFailure
    // and returns false. whatItWasFor is a short note that says which job or
    // jobs the query was about, for the log line.
    bool runQuery(QSqlQuery &query, const char *methodName, const QString &whatItWasFor);

    // Saves errorText for lastErrorText() and logs a warning with
    // methodName, whatItWasFor and errorText. runQuery calls this when a
    // query fails.
    void recordFailure(const char *methodName, const QString &whatItWasFor,
                       const QString &errorText);

    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
