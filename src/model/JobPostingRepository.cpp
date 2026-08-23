#include "JobPostingRepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// One place that knows how a query row becomes a JobPosting.
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
    jobPosting.discoverySource     = row.value(QStringLiteral("discoverySource")).toString();
    jobPosting.discoveredTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("discoveredTimestamp")).toString(), Qt::ISODate);
    jobPosting.externalSourceId    = row.value(QStringLiteral("externalSourceId")).toString();
    jobPosting.postedTimestamp     = QDateTime::fromString(
        row.value(QStringLiteral("postedTimestamp")).toString(), Qt::ISODate);
    jobPosting.isRemoteRole        = row.value(QStringLiteral("isRemoteRole")).toInt() != 0;
    return jobPosting;
}

// Newest first, by the date the EMPLOYER posted — falling back to the date
// Job Crush found it, because a source that omits a posting date should not
// sink to the bottom of the list forever.
const QString newestPostingFirstOrdering = QStringLiteral(
    " ORDER BY CASE WHEN postedTimestamp <> '' THEN postedTimestamp "
    "               ELSE discoveredTimestamp END DESC");

} // namespace

JobPostingRepository::JobPostingRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool JobPostingRepository::insertJobPosting(JobPosting &jobPosting)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO jobPosting "
        "  (companyName, positionTitle, locationText, salaryText,"
        "   sourceUrl, fullDescriptionText, discoverySource, discoveredTimestamp,"
        "   externalSourceId, postedTimestamp, isRemoteRole) "
        "VALUES "
        "  (:companyName, :positionTitle, :locationText, :salaryText,"
        "   :sourceUrl, :fullDescriptionText, :discoverySource, :discoveredTimestamp,"
        "   :externalSourceId, :postedTimestamp, :isRemoteRole)"));

    insertQuery.bindValue(QStringLiteral(":companyName"),         jobPosting.companyName);
    insertQuery.bindValue(QStringLiteral(":positionTitle"),       jobPosting.positionTitle);
    insertQuery.bindValue(QStringLiteral(":locationText"),        jobPosting.locationText);
    insertQuery.bindValue(QStringLiteral(":salaryText"),          jobPosting.salaryText);
    insertQuery.bindValue(QStringLiteral(":sourceUrl"),           jobPosting.sourceUrl);
    insertQuery.bindValue(QStringLiteral(":fullDescriptionText"), jobPosting.fullDescriptionText);
    insertQuery.bindValue(QStringLiteral(":discoverySource"),     jobPosting.discoverySource);
    insertQuery.bindValue(QStringLiteral(":discoveredTimestamp"),
                          jobPosting.discoveredTimestamp.toString(Qt::ISODate));
    insertQuery.bindValue(QStringLiteral(":externalSourceId"),    jobPosting.externalSourceId);
    insertQuery.bindValue(QStringLiteral(":postedTimestamp"),
                          jobPosting.postedTimestamp.isValid()
                              ? jobPosting.postedTimestamp.toString(Qt::ISODate)
                              : QString());
    insertQuery.bindValue(QStringLiteral(":isRemoteRole"), jobPosting.isRemoteRole ? 1 : 0);

    if (!insertQuery.exec()) {
        return false;
    }

    jobPosting.jobPostingId = insertQuery.lastInsertId().toLongLong();
    return true;
}

bool JobPostingRepository::insertDiscoveryIfNew(JobPosting &jobPosting, bool &wasAlreadyKnown)
{
    // Ask first rather than relying on the unique index to reject the insert:
    // "have I seen this before?" is a question with a legitimate answer, not
    // a failure to be caught.
    QSqlQuery existingDiscoveryQuery(jobCrushDatabase.connection());
    existingDiscoveryQuery.prepare(QStringLiteral(
        "SELECT jobPostingId FROM jobPosting "
        "WHERE discoverySource = :discoverySource "
        "  AND externalSourceId = :externalSourceId"));
    existingDiscoveryQuery.bindValue(QStringLiteral(":discoverySource"),
                                     jobPosting.discoverySource);
    existingDiscoveryQuery.bindValue(QStringLiteral(":externalSourceId"),
                                     jobPosting.externalSourceId);

    if (!existingDiscoveryQuery.exec()) {
        wasAlreadyKnown = false;
        return false;
    }

    if (existingDiscoveryQuery.next()) {
        jobPosting.jobPostingId = existingDiscoveryQuery.value(0).toLongLong();
        wasAlreadyKnown = true;
        return true;
    }

    wasAlreadyKnown = false;
    return insertJobPosting(jobPosting);
}

QList<JobPosting> JobPostingRepository::loadAllJobPostings()
{
    QList<JobPosting> allJobPostings;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.exec(QStringLiteral(
        "SELECT * FROM jobPosting ORDER BY discoveredTimestamp DESC"));

    while (selectQuery.next()) {
        allJobPostings.append(jobPostingFromQueryRow(selectQuery));
    }
    return allJobPostings;
}

QList<JobPosting> JobPostingRepository::loadJobPostingsFromSource(
    const QString &discoverySource)
{
    QList<JobPosting> jobPostingsFromSource;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPosting WHERE discoverySource = :discoverySource")
        + newestPostingFirstOrdering);
    selectQuery.bindValue(QStringLiteral(":discoverySource"), discoverySource);
    selectQuery.exec();

    while (selectQuery.next()) {
        jobPostingsFromSource.append(jobPostingFromQueryRow(selectQuery));
    }
    return jobPostingsFromSource;
}

QList<JobPosting> JobPostingRepository::loadAllDiscoveredJobPostings()
{
    QList<JobPosting> discoveredJobPostings;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.exec(QStringLiteral(
        "SELECT * FROM jobPosting WHERE externalSourceId <> ''")
        + newestPostingFirstOrdering);

    while (selectQuery.next()) {
        discoveredJobPostings.append(jobPostingFromQueryRow(selectQuery));
    }
    return discoveredJobPostings;
}

JobPosting JobPostingRepository::loadJobPostingById(qint64 jobPostingId, bool &found)
{
    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPosting WHERE jobPostingId = :jobPostingId"));
    selectQuery.bindValue(QStringLiteral(":jobPostingId"), jobPostingId);
    selectQuery.exec();

    if (selectQuery.next()) {
        found = true;
        return jobPostingFromQueryRow(selectQuery);
    }
    found = false;
    return JobPosting{};
}
