#pragma once

#include <QList>
#include <QString>

#include "JobPosting.h"

class JobCrushDatabase;
class QSqlQuery;

// JobPostingRepository
//
// Reads and writes JobPosting rows in the database. Everything above this
// class works with JobPosting data records. Only this class writes SQL for
// them.
//
// Every method that fails logs what went wrong, which method it was, and
// the job it was working on. lastErrorText() gives the same reason to the
// caller.
class JobPostingRepository {
public:
    explicit JobPostingRepository(JobCrushDatabase &database);

    // Saves a new posting and returns it with jobPostingId filled in.
    // Returns false on database failure.
    bool insertJobPosting(JobPosting &jobPosting);

    // Saves a JobScout discovery, unless this source already delivered this
    // exact job before. wasAlreadyKnown says which happened, so a sweep can
    // report "14 new, 61 already seen".
    //
    // Returns false only when the database fails. A job seen before is a
    // normal result, not an error.
    bool insertDiscoveryIfNew(JobPosting &jobPosting, bool &wasAlreadyKnown);

    // Every posting known to Job Crush, newest first.
    QList<JobPosting> loadAllJobPostings();

    // Everything read off one board or site, newest posting first. This
    // fills that source's tab on the Discoveries page.
    QList<JobPosting> loadJobPostingsFromSource(const QString &postingSource);

    // Marks a job Job Crush already had as added by the user.
    //
    // A sweep often finds a job before the user goes looking for it. When the
    // user then pastes the same job in by hand, it has to show on the Manual
    // Add tab, because that is where Job Crush tells them to look. Without
    // this mark, it would stay filed as a sweep find and not show there.
    bool markJobPostingAsHandAdded(qint64 jobPostingId);

    // Replaces the facts a person can fix by hand: company, title, place,
    // pay, link, and whether it is remote. Used when an outside AI corrects a
    // job in its JobQuest.txt file. Nothing else on the row is touched.
    bool updateJobPostingDetails(const JobPosting &jobPosting);

    // Records when the posting was found missing from the employer's board.
    // An invalid timestamp clears it, for a posting that is listed again.
    bool updatePostingGoneSince(qint64 jobPostingId, const QDateTime &goneSinceTimestamp);

    // Records that the user checked this job and it is still open, and takes
    // off any "gone" mark. Job Crush never marks it gone after this.
    bool markStillOpenByUser(qint64 jobPostingId);

    // Every job the user added by hand, the one added most recently first.
    // This fills the Manual Add tab. It is ordered by when the user added the
    // job, not when the employer posted it, so the newest job added is
    // always the first row.
    QList<JobPosting> loadHandAddedJobPostings();

    // Everything JobScout has ever found, from every source, newest first.
    // Top Prospects ranks this list rather than any single source's.
    QList<JobPosting> loadAllDiscoveredJobPostings();

    // One posting by its id. jobPostingWasFound is false when no row has
    // that id.
    JobPosting loadJobPostingById(qint64 jobPostingId, bool &jobPostingWasFound);

    // Why the last insert, update or query failed.
    QString lastErrorText() const;

    // Saving many jobs one at a time writes to the disk once per job. With
    // thousands of jobs, that froze the window. Call beginManyChanges, make
    // the changes, then call finishManyChanges, and the disk is written once.
    bool beginManyChanges();
    bool finishManyChanges();

private:
    // Runs a query that was already prepared and bound. On failure, keeps the
    // reason for lastErrorText() and logs it with the method name and the
    // job it was about. Returns whether the query ran.
    bool runQuery(QSqlQuery &query, const char *methodName, const QString &whatItWasFor);

    // Logs a failure that is not tied to one query, such as a failed commit.
    void recordFailure(const char *methodName, const QString &whatItWasFor,
                       const QString &errorText);

    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
