#include "JobApplicationRepository.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// An unassigned QString is NULL, not empty, and binding NULL to a NOT NULL
// column makes SQLite refuse the whole row. This trap has now cost this
// project twice — it swallowed every Arbeitnow discovery for days, and then
// it stopped CRUSH putting anything on the board at all, because a campaign
// that has not been applied to yet has no appliedTimestamp. Every repository
// in Job Crush carries this guard for exactly that reason.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QString::fromLatin1("") : possiblyNullText;
}

// One place that knows how a query row becomes a JobApplication.
JobApplication jobApplicationFromQueryRow(const QSqlQuery &row)
{
    JobApplication jobApplication;
    jobApplication.jobApplicationId = row.value(QStringLiteral("jobApplicationId")).toLongLong();
    jobApplication.jobPostingId     = row.value(QStringLiteral("jobPostingId")).toLongLong();
    jobApplication.pipelineStage    = pipelineStageFromStorageText(
        row.value(QStringLiteral("pipelineStage")).toString());
    jobApplication.targetedTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("targetedTimestamp")).toString(), Qt::ISODate);
    jobApplication.appliedTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("appliedTimestamp")).toString(), Qt::ISODate);
    jobApplication.notesText = row.value(QStringLiteral("notesText")).toString();
    return jobApplication;
}

} // namespace

JobApplicationRepository::JobApplicationRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool JobApplicationRepository::insertJobApplication(JobApplication &jobApplication)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO jobApplication "
        "  (jobPostingId, pipelineStage, targetedTimestamp, appliedTimestamp, notesText) "
        "VALUES "
        "  (:jobPostingId, :pipelineStage, :targetedTimestamp, :appliedTimestamp, :notesText)"));

    insertQuery.bindValue(QStringLiteral(":jobPostingId"),  jobApplication.jobPostingId);
    insertQuery.bindValue(QStringLiteral(":pipelineStage"),
                          pipelineStageToStorageText(jobApplication.pipelineStage));
    insertQuery.bindValue(QStringLiteral(":targetedTimestamp"),
                          textOrEmpty(jobApplication.targetedTimestamp.toString(Qt::ISODate)));
    insertQuery.bindValue(QStringLiteral(":appliedTimestamp"),
                          jobApplication.appliedTimestamp.isValid()
                              ? jobApplication.appliedTimestamp.toString(Qt::ISODate)
                              : QString::fromLatin1(""));
    insertQuery.bindValue(QStringLiteral(":notesText"),
                          textOrEmpty(jobApplication.notesText));

    if (!insertQuery.exec()) {
        // A repository that returns false and keeps the reason to itself
        // forces every caller above it to guess. This one cost an afternoon.
        lastErrorDescription = insertQuery.lastError().text();
        return false;
    }

    jobApplication.jobApplicationId = insertQuery.lastInsertId().toLongLong();
    return true;
}

QList<JobApplication> JobApplicationRepository::loadAllJobApplications()
{
    QList<JobApplication> allJobApplications;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.exec(QStringLiteral(
        "SELECT * FROM jobApplication ORDER BY targetedTimestamp ASC"));

    while (selectQuery.next()) {
        allJobApplications.append(jobApplicationFromQueryRow(selectQuery));
    }
    return allJobApplications;
}

bool JobApplicationRepository::updatePipelineStage(qint64 jobApplicationId,
                                                   PipelineStage newPipelineStage)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE jobApplication SET pipelineStage = :pipelineStage "
        "WHERE jobApplicationId = :jobApplicationId"));
    updateQuery.bindValue(QStringLiteral(":pipelineStage"),
                          pipelineStageToStorageText(newPipelineStage));
    updateQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    return updateQuery.exec();
}

bool JobApplicationRepository::updateNotesText(qint64 jobApplicationId,
                                               const QString &newNotesText)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE jobApplication SET notesText = :notesText "
        "WHERE jobApplicationId = :jobApplicationId"));
    updateQuery.bindValue(QStringLiteral(":notesText"), textOrEmpty(newNotesText));
    updateQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    return updateQuery.exec();
}

bool JobApplicationRepository::updateAppliedTimestamp(qint64 jobApplicationId,
                                                      const QDateTime &appliedTimestamp)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE jobApplication SET appliedTimestamp = :appliedTimestamp "
        "WHERE jobApplicationId = :jobApplicationId"));
    updateQuery.bindValue(QStringLiteral(":appliedTimestamp"),
                          appliedTimestamp.isValid()
                              ? appliedTimestamp.toString(Qt::ISODate)
                              : QString::fromLatin1(""));
    updateQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    return updateQuery.exec();
}

bool JobApplicationRepository::removeJobApplication(qint64 jobApplicationId)
{
    QSqlQuery deleteQuery(jobCrushDatabase.connection());
    deleteQuery.prepare(QStringLiteral(
        "DELETE FROM jobApplication WHERE jobApplicationId = :jobApplicationId"));
    deleteQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    return deleteQuery.exec();
}

QString JobApplicationRepository::lastErrorText() const
{
    return lastErrorDescription;
}
