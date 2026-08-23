#include "CareerHistoryRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// An unassigned QString is null, not empty, and binding null to a NOT NULL
// column makes SQLite refuse the row. Every repository carries this guard.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QString::fromLatin1("") : possiblyNullText;
}

WorkExperience workExperienceFromQueryRow(const QSqlQuery &row)
{
    WorkExperience workExperience;
    workExperience.workExperienceId = row.value(QStringLiteral("workExperienceId")).toLongLong();
    workExperience.employerName     = row.value(QStringLiteral("employerName")).toString();
    workExperience.roleTitle        = row.value(QStringLiteral("roleTitle")).toString();
    workExperience.startDateText    = row.value(QStringLiteral("startDateText")).toString();
    workExperience.endDateText      = row.value(QStringLiteral("endDateText")).toString();
    workExperience.summaryText      = row.value(QStringLiteral("summaryText")).toString();
    workExperience.sourceDocumentId = row.value(QStringLiteral("sourceDocumentId")).toLongLong();
    workExperience.sourceLineText   = row.value(QStringLiteral("sourceLineText")).toString();
    workExperience.isConfirmedByUser =
        row.value(QStringLiteral("isConfirmedByUser")).toInt() != 0;
    return workExperience;
}

EducationRecord educationRecordFromQueryRow(const QSqlQuery &row)
{
    EducationRecord educationRecord;
    educationRecord.educationRecordId =
        row.value(QStringLiteral("educationRecordId")).toLongLong();
    educationRecord.schoolName       = row.value(QStringLiteral("schoolName")).toString();
    educationRecord.credentialText   = row.value(QStringLiteral("credentialText")).toString();
    educationRecord.fieldOfStudyText = row.value(QStringLiteral("fieldOfStudyText")).toString();
    educationRecord.startDateText    = row.value(QStringLiteral("startDateText")).toString();
    educationRecord.endDateText      = row.value(QStringLiteral("endDateText")).toString();
    educationRecord.sourceDocumentId =
        row.value(QStringLiteral("sourceDocumentId")).toLongLong();
    educationRecord.sourceLineText   = row.value(QStringLiteral("sourceLineText")).toString();
    educationRecord.isConfirmedByUser =
        row.value(QStringLiteral("isConfirmedByUser")).toInt() != 0;
    return educationRecord;
}

} // namespace

CareerHistoryRepository::CareerHistoryRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool CareerHistoryRepository::insertWorkExperience(WorkExperience &workExperience)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO workExperience "
        "  (employerName, roleTitle, startDateText, endDateText, summaryText,"
        "   sourceDocumentId, sourceLineText, isConfirmedByUser) "
        "VALUES "
        "  (:employerName, :roleTitle, :startDateText, :endDateText, :summaryText,"
        "   :sourceDocumentId, :sourceLineText, :isConfirmedByUser)"));
    insertQuery.bindValue(QStringLiteral(":employerName"), textOrEmpty(workExperience.employerName));
    insertQuery.bindValue(QStringLiteral(":roleTitle"), textOrEmpty(workExperience.roleTitle));
    insertQuery.bindValue(QStringLiteral(":startDateText"), textOrEmpty(workExperience.startDateText));
    insertQuery.bindValue(QStringLiteral(":endDateText"), textOrEmpty(workExperience.endDateText));
    insertQuery.bindValue(QStringLiteral(":summaryText"), textOrEmpty(workExperience.summaryText));
    insertQuery.bindValue(QStringLiteral(":sourceDocumentId"), workExperience.sourceDocumentId);
    insertQuery.bindValue(QStringLiteral(":sourceLineText"), textOrEmpty(workExperience.sourceLineText));
    insertQuery.bindValue(QStringLiteral(":isConfirmedByUser"), workExperience.isConfirmedByUser ? 1 : 0);

    if (!insertQuery.exec()) {
        lastErrorDescription = insertQuery.lastError().text();
        return false;
    }
    workExperience.workExperienceId = insertQuery.lastInsertId().toLongLong();
    return true;
}

bool CareerHistoryRepository::updateWorkExperience(const WorkExperience &workExperience)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE workExperience SET employerName = :employerName, roleTitle = :roleTitle,"
        "  startDateText = :startDateText, endDateText = :endDateText,"
        "  summaryText = :summaryText, isConfirmedByUser = :isConfirmedByUser "
        "WHERE workExperienceId = :workExperienceId"));
    updateQuery.bindValue(QStringLiteral(":employerName"), textOrEmpty(workExperience.employerName));
    updateQuery.bindValue(QStringLiteral(":roleTitle"), textOrEmpty(workExperience.roleTitle));
    updateQuery.bindValue(QStringLiteral(":startDateText"), textOrEmpty(workExperience.startDateText));
    updateQuery.bindValue(QStringLiteral(":endDateText"), textOrEmpty(workExperience.endDateText));
    updateQuery.bindValue(QStringLiteral(":summaryText"), textOrEmpty(workExperience.summaryText));
    updateQuery.bindValue(QStringLiteral(":isConfirmedByUser"), workExperience.isConfirmedByUser ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":workExperienceId"), workExperience.workExperienceId);

    if (!updateQuery.exec()) {
        lastErrorDescription = updateQuery.lastError().text();
        return false;
    }
    return true;
}

bool CareerHistoryRepository::removeWorkExperience(qint64 workExperienceId)
{
    QSqlQuery deleteQuery(jobCrushDatabase.connection());
    deleteQuery.prepare(QStringLiteral(
        "DELETE FROM workExperience WHERE workExperienceId = :workExperienceId"));
    deleteQuery.bindValue(QStringLiteral(":workExperienceId"), workExperienceId);
    if (!deleteQuery.exec()) {
        lastErrorDescription = deleteQuery.lastError().text();
        return false;
    }
    return true;
}

QList<WorkExperience> CareerHistoryRepository::loadAllWorkExperiences()
{
    QList<WorkExperience> allWorkExperiences;
    QSqlQuery selectQuery(jobCrushDatabase.connection());
    // Newest first by the end date TEXT: "Present" sorts above any year, which
    // is exactly where a current job belongs.
    selectQuery.exec(QStringLiteral(
        "SELECT * FROM workExperience ORDER BY endDateText DESC, workExperienceId DESC"));
    while (selectQuery.next()) {
        allWorkExperiences.append(workExperienceFromQueryRow(selectQuery));
    }
    return allWorkExperiences;
}

bool CareerHistoryRepository::insertEducationRecord(EducationRecord &educationRecord)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO educationRecord "
        "  (schoolName, credentialText, fieldOfStudyText, startDateText, endDateText,"
        "   sourceDocumentId, sourceLineText, isConfirmedByUser) "
        "VALUES "
        "  (:schoolName, :credentialText, :fieldOfStudyText, :startDateText, :endDateText,"
        "   :sourceDocumentId, :sourceLineText, :isConfirmedByUser)"));
    insertQuery.bindValue(QStringLiteral(":schoolName"), textOrEmpty(educationRecord.schoolName));
    insertQuery.bindValue(QStringLiteral(":credentialText"), textOrEmpty(educationRecord.credentialText));
    insertQuery.bindValue(QStringLiteral(":fieldOfStudyText"), textOrEmpty(educationRecord.fieldOfStudyText));
    insertQuery.bindValue(QStringLiteral(":startDateText"), textOrEmpty(educationRecord.startDateText));
    insertQuery.bindValue(QStringLiteral(":endDateText"), textOrEmpty(educationRecord.endDateText));
    insertQuery.bindValue(QStringLiteral(":sourceDocumentId"), educationRecord.sourceDocumentId);
    insertQuery.bindValue(QStringLiteral(":sourceLineText"), textOrEmpty(educationRecord.sourceLineText));
    insertQuery.bindValue(QStringLiteral(":isConfirmedByUser"), educationRecord.isConfirmedByUser ? 1 : 0);

    if (!insertQuery.exec()) {
        lastErrorDescription = insertQuery.lastError().text();
        return false;
    }
    educationRecord.educationRecordId = insertQuery.lastInsertId().toLongLong();
    return true;
}

bool CareerHistoryRepository::updateEducationRecord(const EducationRecord &educationRecord)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE educationRecord SET schoolName = :schoolName,"
        "  credentialText = :credentialText, fieldOfStudyText = :fieldOfStudyText,"
        "  startDateText = :startDateText, endDateText = :endDateText,"
        "  isConfirmedByUser = :isConfirmedByUser "
        "WHERE educationRecordId = :educationRecordId"));
    updateQuery.bindValue(QStringLiteral(":schoolName"), textOrEmpty(educationRecord.schoolName));
    updateQuery.bindValue(QStringLiteral(":credentialText"), textOrEmpty(educationRecord.credentialText));
    updateQuery.bindValue(QStringLiteral(":fieldOfStudyText"), textOrEmpty(educationRecord.fieldOfStudyText));
    updateQuery.bindValue(QStringLiteral(":startDateText"), textOrEmpty(educationRecord.startDateText));
    updateQuery.bindValue(QStringLiteral(":endDateText"), textOrEmpty(educationRecord.endDateText));
    updateQuery.bindValue(QStringLiteral(":isConfirmedByUser"), educationRecord.isConfirmedByUser ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":educationRecordId"), educationRecord.educationRecordId);

    if (!updateQuery.exec()) {
        lastErrorDescription = updateQuery.lastError().text();
        return false;
    }
    return true;
}

bool CareerHistoryRepository::removeEducationRecord(qint64 educationRecordId)
{
    QSqlQuery deleteQuery(jobCrushDatabase.connection());
    deleteQuery.prepare(QStringLiteral(
        "DELETE FROM educationRecord WHERE educationRecordId = :educationRecordId"));
    deleteQuery.bindValue(QStringLiteral(":educationRecordId"), educationRecordId);
    if (!deleteQuery.exec()) {
        lastErrorDescription = deleteQuery.lastError().text();
        return false;
    }
    return true;
}

QList<EducationRecord> CareerHistoryRepository::loadAllEducationRecords()
{
    QList<EducationRecord> allEducationRecords;
    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.exec(QStringLiteral(
        "SELECT * FROM educationRecord ORDER BY endDateText DESC, educationRecordId DESC"));
    while (selectQuery.next()) {
        allEducationRecords.append(educationRecordFromQueryRow(selectQuery));
    }
    return allEducationRecords;
}

bool CareerHistoryRepository::removeUnconfirmedEntries()
{
    QSqlQuery deleteWorkQuery(jobCrushDatabase.connection());
    if (!deleteWorkQuery.exec(QStringLiteral(
            "DELETE FROM workExperience WHERE isConfirmedByUser = 0"))) {
        lastErrorDescription = deleteWorkQuery.lastError().text();
        return false;
    }
    QSqlQuery deleteEducationQuery(jobCrushDatabase.connection());
    if (!deleteEducationQuery.exec(QStringLiteral(
            "DELETE FROM educationRecord WHERE isConfirmedByUser = 0"))) {
        lastErrorDescription = deleteEducationQuery.lastError().text();
        return false;
    }
    return true;
}

QString CareerHistoryRepository::lastErrorText() const
{
    return lastErrorDescription;
}
