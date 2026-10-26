#include "JobPostingRepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// Copies each column of one query result row into a new JobPosting and
// returns it. Every load method in this file calls this function, so the
// match between columns and fields is written in only one place.
JobPosting jobPostingFromQueryRow(const QSqlQuery &row)
{
    JobPosting jobPosting;
    jobPosting.jobPostingId        = row.value(QStringLiteral("jobPostingId")).toLongLong();
    jobPosting.companyName         = row.value(QStringLiteral("companyName")).toString();
    jobPosting.positionTitle       = row.value(QStringLiteral("positionTitle")).toString();
    jobPosting.locationText        = row.value(QStringLiteral("locationText")).toString();
    jobPosting.salaryText          = row.value(QStringLiteral("salaryText")).toString();
    jobPosting.sourceUrl           = row.value(QStringLiteral("sourceUrl")).toString();
    jobPosting.fullDescriptionText = row.value(QStringLiteral("fullDescriptionText")).toString();
    jobPosting.postingSource       = row.value(QStringLiteral("postingSource")).toString();
    jobPosting.scoutSource         = row.value(QStringLiteral("scoutSource")).toString();
    jobPosting.discoveredTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("discoveredTimestamp")).toString(), Qt::ISODate);
    jobPosting.externalSourceId    = row.value(QStringLiteral("externalSourceId")).toString();
    jobPosting.postedTimestamp     = QDateTime::fromString(
        row.value(QStringLiteral("postedTimestamp")).toString(), Qt::ISODate);
    jobPosting.isRemoteRole        = row.value(QStringLiteral("isRemoteRole")).toInt() != 0;
    jobPosting.postingGoneSinceTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("postingGoneSinceTimestamp")).toString(), Qt::ISODate);
    jobPosting.userSaysStillOpenTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("userSaysStillOpenTimestamp")).toString(), Qt::ISODate);
    return jobPosting;
}

// Returns an empty string if possiblyNullText is NULL. Otherwise returns the
// text unchanged. A QString that was never given a value is NULL, not empty.
// If code binds a NULL to a NOT NULL column, SQLite refuses the whole row.
//
// This really happened. Arbeitnow, a job site, sends no salary field, so
// every posting from it had a NULL salaryText, and every insert was refused.
// About 175 jobs a day were lost, and the only sign was an empty tab. Text
// fields pass through this function before they are bound, so the fix is in
// one place.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QStringLiteral("") : possiblyNullText;
}

// Returns the timestamp as ISO 8601 text, which is how the database stores
// dates. If the timestamp is invalid, returns an empty string. The database
// uses an empty string to mean "no date".
QString timestampTextOrEmpty(const QDateTime &timestamp)
{
    return timestamp.isValid() ? timestamp.toString(Qt::ISODate) : QStringLiteral("");
}

// An ORDER BY clause that some load methods add to the end of their SELECT.
// It sorts newest first by postedTimestamp, the date the EMPLOYER posted the
// job. If a posting has no posted date, it uses discoveredTimestamp, the
// date Job Crush found the job. Without that, every posting with no posted
// date would always be at the bottom of the list.
const QString newestPostingFirstOrdering = QStringLiteral(
    " ORDER BY CASE WHEN postedTimestamp <> '' THEN postedTimestamp "
    "               ELSE discoveredTimestamp END DESC");

// Returns a short name for a job posting, for log lines. It gives the title
// and company. If the posting has an id (jobPostingId is not 0), it adds the
// id too.
QString describeJobPosting(const JobPosting &jobPosting)
{
    const QString titleAndCompany = QStringLiteral("\"%1\" at %2")
                                        .arg(jobPosting.positionTitle, jobPosting.companyName);
    if (jobPosting.jobPostingId == 0) {
        return titleAndCompany;
    }
    return QStringLiteral("job posting %1, %2").arg(jobPosting.jobPostingId)
                                                .arg(titleAndCompany);
}

} // namespace

JobPostingRepository::JobPostingRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool JobPostingRepository::runQuery(QSqlQuery &query, const char *methodName,
                                    const QString &whatItWasFor)
{
    if (query.exec()) {
        return true;
    }
    recordFailure(methodName, whatItWasFor, query.lastError().text());
    return false;
}

void JobPostingRepository::recordFailure(const char *methodName,
                                         const QString &whatItWasFor,
                                         const QString &errorText)
{
    lastErrorDescription = errorText;
    qWarning().noquote() << QStringLiteral("JobPostingRepository::%1: failed for %2 — %3")
                                .arg(QLatin1String(methodName), whatItWasFor, errorText);
}

bool JobPostingRepository::insertJobPosting(JobPosting &jobPosting)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO jobPosting "
        "  (companyName, positionTitle, locationText, salaryText,"
        "   sourceUrl, fullDescriptionText, postingSource, scoutSource,"
        "   discoveredTimestamp,"
        "   externalSourceId, postedTimestamp, isRemoteRole) "
        "VALUES "
        "  (:companyName, :positionTitle, :locationText, :salaryText,"
        "   :sourceUrl, :fullDescriptionText, :postingSource, :scoutSource,"
        "   :discoveredTimestamp,"
        "   :externalSourceId, :postedTimestamp, :isRemoteRole)"));

    insertQuery.bindValue(QStringLiteral(":companyName"),
                          textOrEmpty(jobPosting.companyName));
    insertQuery.bindValue(QStringLiteral(":positionTitle"),
                          textOrEmpty(jobPosting.positionTitle));
    insertQuery.bindValue(QStringLiteral(":locationText"),
                          textOrEmpty(jobPosting.locationText));
    insertQuery.bindValue(QStringLiteral(":salaryText"),
                          textOrEmpty(jobPosting.salaryText));
    insertQuery.bindValue(QStringLiteral(":sourceUrl"),
                          textOrEmpty(jobPosting.sourceUrl));
    insertQuery.bindValue(QStringLiteral(":fullDescriptionText"),
                          textOrEmpty(jobPosting.fullDescriptionText));
    insertQuery.bindValue(QStringLiteral(":postingSource"),
                          textOrEmpty(jobPosting.postingSource));
    // If scoutSource is empty, this saves "scout", which means a JobScout
    // sweep found the job. An empty string would not tell anyone whether
    // the user or a sweep brought the job in.
    insertQuery.bindValue(QStringLiteral(":scoutSource"),
                          jobPosting.scoutSource.trimmed().isEmpty()
                              ? ScoutSourceText::Scout
                              : jobPosting.scoutSource);
    insertQuery.bindValue(QStringLiteral(":discoveredTimestamp"),
                          timestampTextOrEmpty(jobPosting.discoveredTimestamp));
    insertQuery.bindValue(QStringLiteral(":externalSourceId"),
                          textOrEmpty(jobPosting.externalSourceId));
    insertQuery.bindValue(QStringLiteral(":postedTimestamp"),
                          timestampTextOrEmpty(jobPosting.postedTimestamp));
    insertQuery.bindValue(QStringLiteral(":isRemoteRole"), jobPosting.isRemoteRole ? 1 : 0);

    if (!runQuery(insertQuery, "insertJobPosting", describeJobPosting(jobPosting))) {
        return false;
    }

    jobPosting.jobPostingId = insertQuery.lastInsertId().toLongLong();
    return true;
}

bool JobPostingRepository::insertDiscoveryIfNew(JobPosting &jobPosting, bool &wasAlreadyKnown)
{
    wasAlreadyKnown = false;

    // If the source gave no id for this job (externalSourceId is empty),
    // this code cannot tell whether the job is already stored. The
    // uniquePostingPerBoard index also skips rows with no id. So the job is
    // always saved as a new row.
    if (textOrEmpty(jobPosting.externalSourceId).isEmpty()) {
        return insertJobPosting(jobPosting);
    }

    // Before inserting, this looks for a stored row with the same
    // postingSource and externalSourceId. The uniquePostingPerBoard index
    // would refuse a duplicate insert, but that refusal is a database error.
    // A job seen before is a normal result, so this code looks first and
    // does not report an error.
    //
    // The line "externalSourceId <> ''" looks useless, because the code
    // above already returned if the id is empty. It is needed anyway. The
    // uniquePostingPerBoard index covers only rows where externalSourceId is
    // not empty, and SQLite uses that index only if the query has the same
    // condition. Without the line, SQLite reads the whole table for every
    // job saved. With thousands of jobs, that made a sweep very slow.
    QSqlQuery existingDiscoveryQuery(jobCrushDatabase.connection());
    existingDiscoveryQuery.prepare(QStringLiteral(
        "SELECT jobPostingId FROM jobPosting "
        "WHERE postingSource = :postingSource "
        "  AND externalSourceId = :externalSourceId "
        "  AND externalSourceId <> ''"));
    // This passes postingSource through textOrEmpty, the same way
    // insertJobPosting does. So a NULL source name is stored as "" and
    // looked up as "". In SQLite, NULL never equals anything, not even
    // another NULL. If this lookup bound a NULL, it would never find the row
    // the insert stored with "". Then every sweep would try to insert the
    // same job again, and the uniquePostingPerBoard index would refuse it
    // as an error.
    existingDiscoveryQuery.bindValue(QStringLiteral(":postingSource"),
                                     textOrEmpty(jobPosting.postingSource));
    existingDiscoveryQuery.bindValue(QStringLiteral(":externalSourceId"),
                                     textOrEmpty(jobPosting.externalSourceId));

    if (!runQuery(existingDiscoveryQuery, "insertDiscoveryIfNew",
                  describeJobPosting(jobPosting))) {
        return false;
    }

    if (existingDiscoveryQuery.next()) {
        jobPosting.jobPostingId = existingDiscoveryQuery.value(0).toLongLong();
        wasAlreadyKnown = true;
        return true;
    }

    return insertJobPosting(jobPosting);
}

QList<JobPosting> JobPostingRepository::loadAllJobPostings()
{
    QList<JobPosting> allJobPostings;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPosting ORDER BY discoveredTimestamp DESC"));
    if (!runQuery(selectQuery, "loadAllJobPostings", QStringLiteral("every job posting"))) {
        return allJobPostings;
    }

    while (selectQuery.next()) {
        allJobPostings.append(jobPostingFromQueryRow(selectQuery));
    }
    return allJobPostings;
}

QList<JobPosting> JobPostingRepository::loadJobPostingsFromSource(
    const QString &postingSource)
{
    QList<JobPosting> jobPostingsFromSource;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPosting WHERE postingSource = :postingSource")
        + newestPostingFirstOrdering);
    selectQuery.bindValue(QStringLiteral(":postingSource"), postingSource);
    if (!runQuery(selectQuery, "loadJobPostingsFromSource",
                  QStringLiteral("source \"%1\"").arg(postingSource))) {
        return jobPostingsFromSource;
    }

    while (selectQuery.next()) {
        jobPostingsFromSource.append(jobPostingFromQueryRow(selectQuery));
    }
    return jobPostingsFromSource;
}

bool JobPostingRepository::markJobPostingAsHandAdded(qint64 jobPostingId)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE jobPosting SET scoutSource = :scoutSource "
        "WHERE jobPostingId = :jobPostingId"));
    updateQuery.bindValue(QStringLiteral(":scoutSource"), ScoutSourceText::You);
    updateQuery.bindValue(QStringLiteral(":jobPostingId"), jobPostingId);

    return runQuery(updateQuery, "markJobPostingAsHandAdded",
                    QStringLiteral("job posting %1").arg(jobPostingId));
}

bool JobPostingRepository::updateJobPostingDetails(const JobPosting &jobPosting)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE jobPosting SET "
        "  companyName = :companyName, "
        "  positionTitle = :positionTitle, "
        "  locationText = :locationText, "
        "  salaryText = :salaryText, "
        "  sourceUrl = :sourceUrl, "
        "  isRemoteRole = :isRemoteRole "
        "WHERE jobPostingId = :jobPostingId"));
    updateQuery.bindValue(QStringLiteral(":companyName"), textOrEmpty(jobPosting.companyName));
    updateQuery.bindValue(QStringLiteral(":positionTitle"),
                          textOrEmpty(jobPosting.positionTitle));
    updateQuery.bindValue(QStringLiteral(":locationText"), textOrEmpty(jobPosting.locationText));
    updateQuery.bindValue(QStringLiteral(":salaryText"), textOrEmpty(jobPosting.salaryText));
    updateQuery.bindValue(QStringLiteral(":sourceUrl"), textOrEmpty(jobPosting.sourceUrl));
    updateQuery.bindValue(QStringLiteral(":isRemoteRole"), jobPosting.isRemoteRole ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":jobPostingId"), jobPosting.jobPostingId);

    return runQuery(updateQuery, "updateJobPostingDetails", describeJobPosting(jobPosting));
}

bool JobPostingRepository::updatePostingGoneSince(qint64 jobPostingId,
                                                  const QDateTime &goneSinceTimestamp)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE jobPosting SET postingGoneSinceTimestamp = :postingGoneSinceTimestamp "
        "WHERE jobPostingId = :jobPostingId "
        // If the user has said this job is still open, this UPDATE does not
        // set the "gone" mark. It can still clear the mark. The caller looks
        // at this before it asks the employer's board if the job is still
        // listed. It is checked again here because the user can press
        // "still open" while that request waits on the network.
        "  AND (:isClearingTheMark = 1 OR userSaysStillOpenTimestamp = '')"));
    updateQuery.bindValue(QStringLiteral(":postingGoneSinceTimestamp"),
                          timestampTextOrEmpty(goneSinceTimestamp));
    updateQuery.bindValue(QStringLiteral(":isClearingTheMark"),
                          goneSinceTimestamp.isValid() ? 0 : 1);
    updateQuery.bindValue(QStringLiteral(":jobPostingId"), jobPostingId);

    return runQuery(updateQuery, "updatePostingGoneSince",
                    QStringLiteral("job posting %1").arg(jobPostingId));
}

bool JobPostingRepository::markStillOpenByUser(qint64 jobPostingId)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE jobPosting SET userSaysStillOpenTimestamp = :userSaysStillOpenTimestamp, "
        "  postingGoneSinceTimestamp = '' "
        "WHERE jobPostingId = :jobPostingId"));
    updateQuery.bindValue(QStringLiteral(":userSaysStillOpenTimestamp"),
                          timestampTextOrEmpty(QDateTime::currentDateTime()));
    updateQuery.bindValue(QStringLiteral(":jobPostingId"), jobPostingId);

    return runQuery(updateQuery, "markStillOpenByUser",
                    QStringLiteral("job posting %1").arg(jobPostingId));
}

QList<JobPosting> JobPostingRepository::loadHandAddedJobPostings()
{
    QList<JobPosting> handAddedJobPostings;

    // Sorted newest first by discoveredTimestamp, the time Job Crush first
    // stored the job, not by the date the employer posted it. If the user
    // added an old posting a minute ago, it is the job they are looking for,
    // so it goes at the top. For a job a sweep stored first and the user
    // pasted in later, discoveredTimestamp is still the time of the sweep.
    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPosting WHERE scoutSource = :scoutSource "
        "ORDER BY discoveredTimestamp DESC, jobPostingId DESC"));
    selectQuery.bindValue(QStringLiteral(":scoutSource"), ScoutSourceText::You);
    if (!runQuery(selectQuery, "loadHandAddedJobPostings",
                  QStringLiteral("the hand-added job postings"))) {
        return handAddedJobPostings;
    }

    while (selectQuery.next()) {
        handAddedJobPostings.append(jobPostingFromQueryRow(selectQuery));
    }
    return handAddedJobPostings;
}

QList<JobPosting> JobPostingRepository::loadAllDiscoveredJobPostings()
{
    QList<JobPosting> discoveredJobPostings;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPosting WHERE externalSourceId <> ''")
        + newestPostingFirstOrdering);
    if (!runQuery(selectQuery, "loadAllDiscoveredJobPostings",
                  QStringLiteral("every discovered job posting"))) {
        return discoveredJobPostings;
    }

    while (selectQuery.next()) {
        discoveredJobPostings.append(jobPostingFromQueryRow(selectQuery));
    }
    return discoveredJobPostings;
}

JobPosting JobPostingRepository::loadJobPostingById(qint64 jobPostingId,
                                                    bool &jobPostingWasFound)
{
    jobPostingWasFound = false;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPosting WHERE jobPostingId = :jobPostingId"));
    selectQuery.bindValue(QStringLiteral(":jobPostingId"), jobPostingId);
    if (!runQuery(selectQuery, "loadJobPostingById",
                  QStringLiteral("job posting %1").arg(jobPostingId))) {
        return JobPosting{};
    }

    if (!selectQuery.next()) {
        return JobPosting{};
    }
    jobPostingWasFound = true;
    return jobPostingFromQueryRow(selectQuery);
}

QString JobPostingRepository::lastErrorText() const
{
    return lastErrorDescription;
}

bool JobPostingRepository::beginManyChanges()
{
    // Job Crush has two connections to the database. The screen's thread
    // uses one. A background thread that saves and ranks jobs uses the
    // other. BEGIN IMMEDIATE starts a transaction and takes the write lock
    // right away. If the other connection is writing, it waits for it. A
    // plain BEGIN does not take the write lock at the start. If the other
    // connection then writes after this transaction's first read, SQLite
    // refuses every insert in the transaction with "database is locked".
    QSqlQuery beginQuery(jobCrushDatabase.connection());
    beginQuery.prepare(QStringLiteral("BEGIN IMMEDIATE"));
    return runQuery(beginQuery, "beginManyChanges",
                    QStringLiteral("starting a batch of changes"));
}

bool JobPostingRepository::finishManyChanges()
{
    QSqlQuery commitQuery(jobCrushDatabase.connection());
    commitQuery.prepare(QStringLiteral("COMMIT"));
    return runQuery(commitQuery, "finishManyChanges",
                    QStringLiteral("saving a batch of changes"));
}
