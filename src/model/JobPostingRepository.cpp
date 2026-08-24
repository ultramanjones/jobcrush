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

// A QString that was never assigned is NULL, not empty — and binding a null
// to a NOT NULL column makes SQLite reject the whole row.
//
// This is not a hypothetical: Arbeitnow publishes no salary field, so every
// posting from it arrived with an unassigned salaryText and every insert was
// refused. 175 jobs a day silently hit the floor, and the only visible symptom
// was an empty tab. Any field a source does not publish takes this path, so
// the conversion belongs here, once, rather than at each call site where the
// next missing field would be missed again.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QString::fromLatin1("") : possiblyNullText;
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
    insertQuery.bindValue(QStringLiteral(":discoverySource"),
                          textOrEmpty(jobPosting.discoverySource));
    insertQuery.bindValue(QStringLiteral(":discoveredTimestamp"),
                          textOrEmpty(jobPosting.discoveredTimestamp.toString(Qt::ISODate)));
    insertQuery.bindValue(QStringLiteral(":externalSourceId"),
                          textOrEmpty(jobPosting.externalSourceId));
    insertQuery.bindValue(QStringLiteral(":postedTimestamp"),
                          jobPosting.postedTimestamp.isValid()
                              ? jobPosting.postedTimestamp.toString(Qt::ISODate)
                              : QString::fromLatin1(""));
    insertQuery.bindValue(QStringLiteral(":isRemoteRole"), jobPosting.isRemoteRole ? 1 : 0);

    if (!insertQuery.exec()) {
        lastErrorDescription = insertQuery.lastError().text();
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
    // textOrEmpty on BOTH, same as the insert.
    //
    // A QString nobody assigned is null, and a null binds as SQL NULL — and
    // in SQLite nothing equals NULL, not even NULL. So the lookup would miss
    // every time, the insert would store "" instead, and the same job would
    // be stored again on every sweep, forever. The insert learned this lesson
    // already; the lookup beside it did not.
    existingDiscoveryQuery.bindValue(QStringLiteral(":discoverySource"),
                                     textOrEmpty(jobPosting.discoverySource));
    existingDiscoveryQuery.bindValue(QStringLiteral(":externalSourceId"),
                                     textOrEmpty(jobPosting.externalSourceId));

    if (!existingDiscoveryQuery.exec()) {
        lastErrorDescription = existingDiscoveryQuery.lastError().text();
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

QString JobPostingRepository::lastErrorText() const
{
    return lastErrorDescription;
}
