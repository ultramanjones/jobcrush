#include "JobPostingRepository.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// The one place that turns a query row into a JobPosting.
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
    return jobPosting;
}

// A QString that was never given a value is NULL, not empty. Binding a NULL
// to a NOT NULL column makes SQLite refuse the whole row.
//
// This really happened. Arbeitnow sends no salary field, so every posting
// from it had a NULL salaryText, and every insert was refused. About 175
// jobs a day were lost, and the only sign was an empty tab. Any field a
// source does not send goes through here, so the fix lives in one place.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QStringLiteral("") : possiblyNullText;
}

// A date as stored text, or an empty string for "no date".
QString timestampTextOrEmpty(const QDateTime &timestamp)
{
    return timestamp.isValid() ? timestamp.toString(Qt::ISODate) : QStringLiteral("");
}

// Newest first, by the date the EMPLOYER posted the job. A posting with no
// posting date uses the date Job Crush found it instead, so it does not sink
// to the bottom of the list for good.
const QString newestPostingFirstOrdering = QStringLiteral(
    " ORDER BY CASE WHEN postedTimestamp <> '' THEN postedTimestamp "
    "               ELSE discoveredTimestamp END DESC");

// How a posting is named in a log line: its title and company, and its id
// once it has one.
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
    // A posting with no scout source came from a sweep. An empty string would
    // tell nobody who found it.
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

    // Look first, instead of letting the unique index refuse the insert. A
    // job seen before is a normal answer, and should not show up as an error.
    QSqlQuery existingDiscoveryQuery(jobCrushDatabase.connection());
    existingDiscoveryQuery.prepare(QStringLiteral(
        "SELECT jobPostingId FROM jobPosting "
        "WHERE postingSource = :postingSource "
        "  AND externalSourceId = :externalSourceId"));
    // textOrEmpty on both, the same as the insert does.
    //
    // A NULL never equals anything in SQLite, not even another NULL. Without
    // textOrEmpty here, this lookup would never find the row the insert
    // stored with "", and the same job would be stored again on every sweep.
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
        "WHERE jobPostingId = :jobPostingId"));
    updateQuery.bindValue(QStringLiteral(":postingGoneSinceTimestamp"),
                          timestampTextOrEmpty(goneSinceTimestamp));
    updateQuery.bindValue(QStringLiteral(":jobPostingId"), jobPostingId);

    return runQuery(updateQuery, "updatePostingGoneSince",
                    QStringLiteral("job posting %1").arg(jobPostingId));
}

QList<JobPosting> JobPostingRepository::loadHandAddedJobPostings()
{
    QList<JobPosting> handAddedJobPostings;

    // Ordered by when the USER added the job, not when the employer posted
    // it. An old posting added a minute ago is still the one the user is
    // looking for.
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
    if (!jobCrushDatabase.connection().transaction()) {
        recordFailure("beginManyChanges", QStringLiteral("starting a batch of changes"),
                      jobCrushDatabase.connection().lastError().text());
        return false;
    }
    return true;
}

bool JobPostingRepository::finishManyChanges()
{
    if (!jobCrushDatabase.connection().commit()) {
        recordFailure("finishManyChanges", QStringLiteral("saving a batch of changes"),
                      jobCrushDatabase.connection().lastError().text());
        return false;
    }
    return true;
}
