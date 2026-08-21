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
    return jobPosting;
}

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
        "   sourceUrl, fullDescriptionText, discoverySource, discoveredTimestamp) "
        "VALUES "
        "  (:companyName, :positionTitle, :locationText, :salaryText,"
        "   :sourceUrl, :fullDescriptionText, :discoverySource, :discoveredTimestamp)"));

    insertQuery.bindValue(QStringLiteral(":companyName"),         jobPosting.companyName);
    insertQuery.bindValue(QStringLiteral(":positionTitle"),       jobPosting.positionTitle);
    insertQuery.bindValue(QStringLiteral(":locationText"),        jobPosting.locationText);
    insertQuery.bindValue(QStringLiteral(":salaryText"),          jobPosting.salaryText);
    insertQuery.bindValue(QStringLiteral(":sourceUrl"),           jobPosting.sourceUrl);
    insertQuery.bindValue(QStringLiteral(":fullDescriptionText"), jobPosting.fullDescriptionText);
    insertQuery.bindValue(QStringLiteral(":discoverySource"),     jobPosting.discoverySource);
    insertQuery.bindValue(QStringLiteral(":discoveredTimestamp"),
                          jobPosting.discoveredTimestamp.toString(Qt::ISODate));

    if (!insertQuery.exec()) {
        return false;
    }

    jobPosting.jobPostingId = insertQuery.lastInsertId().toLongLong();
    return true;
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
