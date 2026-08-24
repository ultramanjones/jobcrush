#include "CareerHistoryRepository.h"

#include <QHash>
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
    workExperience.wasEditedByUser =
        row.value(QStringLiteral("wasEditedByUser")).toInt() != 0;
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
    educationRecord.wasEditedByUser =
        row.value(QStringLiteral("wasEditedByUser")).toInt() != 0;
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
        "   sourceDocumentId, sourceLineText, isConfirmedByUser, wasEditedByUser) "
        "VALUES "
        "  (:employerName, :roleTitle, :startDateText, :endDateText, :summaryText,"
        "   :sourceDocumentId, :sourceLineText, :isConfirmedByUser, :wasEditedByUser)"));
    insertQuery.bindValue(QStringLiteral(":employerName"), textOrEmpty(workExperience.employerName));
    insertQuery.bindValue(QStringLiteral(":roleTitle"), textOrEmpty(workExperience.roleTitle));
    insertQuery.bindValue(QStringLiteral(":startDateText"), textOrEmpty(workExperience.startDateText));
    insertQuery.bindValue(QStringLiteral(":endDateText"), textOrEmpty(workExperience.endDateText));
    insertQuery.bindValue(QStringLiteral(":summaryText"), textOrEmpty(workExperience.summaryText));
    insertQuery.bindValue(QStringLiteral(":sourceDocumentId"), workExperience.sourceDocumentId);
    insertQuery.bindValue(QStringLiteral(":sourceLineText"), textOrEmpty(workExperience.sourceLineText));
    insertQuery.bindValue(QStringLiteral(":isConfirmedByUser"), workExperience.isConfirmedByUser ? 1 : 0);
    insertQuery.bindValue(QStringLiteral(":wasEditedByUser"), workExperience.wasEditedByUser ? 1 : 0);

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
        "  summaryText = :summaryText, isConfirmedByUser = :isConfirmedByUser, "
        "  wasEditedByUser = :wasEditedByUser "
        "WHERE workExperienceId = :workExperienceId"));
    updateQuery.bindValue(QStringLiteral(":employerName"), textOrEmpty(workExperience.employerName));
    updateQuery.bindValue(QStringLiteral(":roleTitle"), textOrEmpty(workExperience.roleTitle));
    updateQuery.bindValue(QStringLiteral(":startDateText"), textOrEmpty(workExperience.startDateText));
    updateQuery.bindValue(QStringLiteral(":endDateText"), textOrEmpty(workExperience.endDateText));
    updateQuery.bindValue(QStringLiteral(":summaryText"), textOrEmpty(workExperience.summaryText));
    updateQuery.bindValue(QStringLiteral(":isConfirmedByUser"), workExperience.isConfirmedByUser ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":wasEditedByUser"), workExperience.wasEditedByUser ? 1 : 0);
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
    insertQuery.bindValue(QStringLiteral(":wasEditedByUser"), educationRecord.wasEditedByUser ? 1 : 0);

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
        "  isConfirmedByUser = :isConfirmedByUser, wasEditedByUser = :wasEditedByUser "
        "WHERE educationRecordId = :educationRecordId"));
    updateQuery.bindValue(QStringLiteral(":schoolName"), textOrEmpty(educationRecord.schoolName));
    updateQuery.bindValue(QStringLiteral(":credentialText"), textOrEmpty(educationRecord.credentialText));
    updateQuery.bindValue(QStringLiteral(":fieldOfStudyText"), textOrEmpty(educationRecord.fieldOfStudyText));
    updateQuery.bindValue(QStringLiteral(":startDateText"), textOrEmpty(educationRecord.startDateText));
    updateQuery.bindValue(QStringLiteral(":endDateText"), textOrEmpty(educationRecord.endDateText));
    updateQuery.bindValue(QStringLiteral(":isConfirmedByUser"), educationRecord.isConfirmedByUser ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":wasEditedByUser"), educationRecord.wasEditedByUser ? 1 : 0);
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

bool CareerHistoryRepository::workExperienceAlreadyRecorded(
    const WorkExperience &workExperience)
{
    // A blank source line means the user typed this in themselves. Two of
    // those are two real entries, not a duplicate.
    if (workExperience.sourceLineText.trimmed().isEmpty()) {
        return false;
    }
    // Two tests, because a repeat arrives in two different ways.
    //
    // The first is the same line read twice out of the same document. The
    // second is the same JOB arriving from a different document — somebody
    // drops resume.pdf, looks at what came out, and drops it again; or keeps
    // "resume.pdf" and "resume final.pdf", which are one resume with two
    // names. Each drop is its own document, so the first test never fires and
    // the page fills up with pairs. What makes two entries the same thing is
    // what they SAY, not which file they came out of.
    //
    // Only readings suppress readings: a row with no source line was typed by
    // a person, and nothing the reader finds is allowed to silence it.
    QSqlQuery lookupQuery(jobCrushDatabase.connection());
    lookupQuery.prepare(QStringLiteral(
        "SELECT 1 FROM workExperience "
        "WHERE (sourceDocumentId = :sourceDocumentId "
        "       AND sourceLineText = :sourceLineText "
        "       AND roleTitle      = :roleTitle "
        "       AND employerName   = :employerName) "
        "   OR (sourceLineText <> '' "
        "       AND roleTitle      = :roleTitle "
        "       AND employerName   = :employerName "
        "       AND startDateText  = :startDateText "
        "       AND endDateText    = :endDateText) "
        "LIMIT 1"));
    lookupQuery.bindValue(QStringLiteral(":sourceDocumentId"), workExperience.sourceDocumentId);
    lookupQuery.bindValue(QStringLiteral(":sourceLineText"),
                          textOrEmpty(workExperience.sourceLineText));
    lookupQuery.bindValue(QStringLiteral(":roleTitle"), textOrEmpty(workExperience.roleTitle));
    lookupQuery.bindValue(QStringLiteral(":employerName"),
                          textOrEmpty(workExperience.employerName));
    lookupQuery.bindValue(QStringLiteral(":startDateText"),
                          textOrEmpty(workExperience.startDateText));
    lookupQuery.bindValue(QStringLiteral(":endDateText"),
                          textOrEmpty(workExperience.endDateText));
    if (!lookupQuery.exec()) {
        lastErrorDescription = lookupQuery.lastError().text();
        return false;
    }
    return lookupQuery.next();
}

bool CareerHistoryRepository::educationRecordAlreadyRecorded(
    const EducationRecord &educationRecord)
{
    if (educationRecord.sourceLineText.trimmed().isEmpty()) {
        return false;
    }
    // The same two tests as above: the same line twice from one document, or
    // the same schooling from a second copy of the same resume.
    QSqlQuery lookupQuery(jobCrushDatabase.connection());
    lookupQuery.prepare(QStringLiteral(
        "SELECT 1 FROM educationRecord "
        "WHERE (sourceDocumentId = :sourceDocumentId "
        "       AND sourceLineText = :sourceLineText "
        "       AND schoolName     = :schoolName) "
        "   OR (sourceLineText <> '' "
        "       AND schoolName       = :schoolName "
        "       AND credentialText   = :credentialText "
        "       AND fieldOfStudyText = :fieldOfStudyText "
        "       AND startDateText    = :startDateText "
        "       AND endDateText      = :endDateText) "
        "LIMIT 1"));
    lookupQuery.bindValue(QStringLiteral(":sourceDocumentId"),
                          educationRecord.sourceDocumentId);
    lookupQuery.bindValue(QStringLiteral(":sourceLineText"),
                          textOrEmpty(educationRecord.sourceLineText));
    lookupQuery.bindValue(QStringLiteral(":schoolName"),
                          textOrEmpty(educationRecord.schoolName));
    lookupQuery.bindValue(QStringLiteral(":credentialText"),
                          textOrEmpty(educationRecord.credentialText));
    lookupQuery.bindValue(QStringLiteral(":fieldOfStudyText"),
                          textOrEmpty(educationRecord.fieldOfStudyText));
    lookupQuery.bindValue(QStringLiteral(":startDateText"),
                          textOrEmpty(educationRecord.startDateText));
    lookupQuery.bindValue(QStringLiteral(":endDateText"),
                          textOrEmpty(educationRecord.endDateText));
    if (!lookupQuery.exec()) {
        lastErrorDescription = lookupQuery.lastError().text();
        return false;
    }
    return lookupQuery.next();
}

bool CareerHistoryRepository::removeEntriesTheReaderProducedAndNobodyTouched()
{
    // Everything that came OUT OF A DOCUMENT and that no person has typed
    // into since. A better reader is entitled to redo its own work; it is
    // never entitled to redo somebody else's.
    //
    // Note what is NOT protected here: merely CONFIRMED entries. Ticking a
    // box says "your reading is right", and when the reading turns out to
    // have been wrong the tick was agreement with a mistake — keeping it
    // would preserve the mistake and hide the fix.
    for (const QString &tableName : { QStringLiteral("workExperience"),
                                      QStringLiteral("educationRecord") }) {
        QSqlQuery deleteQuery(jobCrushDatabase.connection());
        if (!deleteQuery.exec(QStringLiteral(
                "DELETE FROM %1 WHERE sourceDocumentId > 0 AND wasEditedByUser = 0")
                    .arg(tableName))) {
            lastErrorDescription = deleteQuery.lastError().text();
            return false;
        }
    }
    return true;
}

int CareerHistoryRepository::removeDuplicateEntries()
{
    // Written as a read, a decision and a delete rather than as one clever
    // SQL statement. The rule — "keep the row the user confirmed, otherwise
    // the oldest" — has to be readable by whoever inherits it, and a nested
    // GROUP BY with a correlated subquery is not that.
    int rowsRemoved = 0;

    // --- Jobs ------------------------------------------------------------
    {
        QHash<QString, WorkExperience> survivorByIdentity;
        QList<qint64> identifiersToRemove;

        for (const WorkExperience &workExperience : loadAllWorkExperiences()) {
            // What makes two rows the same job: same employer, same title,
            // same span. Not the source line — the duplicates already in the
            // wild were made before the guard existed, and matching on what a
            // row SAYS catches those too.
            const QString identity = workExperience.employerName.trimmed().toLower()
                + QStringLiteral("\n") + workExperience.roleTitle.trimmed().toLower()
                + QStringLiteral("\n") + workExperience.startDateText.trimmed().toLower()
                + QStringLiteral("\n") + workExperience.endDateText.trimmed().toLower();

            // An entry with nothing in it is not a duplicate of another empty
            // one — those are two blank rows somebody added by hand and is
            // half way through filling in.
            if (identity.trimmed().isEmpty()) {
                continue;
            }

            if (!survivorByIdentity.contains(identity)) {
                survivorByIdentity.insert(identity, workExperience);
                continue;
            }

            const WorkExperience standingSurvivor = survivorByIdentity.value(identity);
            const bool newcomerWins = workExperience.isConfirmedByUser
                && !standingSurvivor.isConfirmedByUser;
            if (newcomerWins) {
                identifiersToRemove.append(standingSurvivor.workExperienceId);
                survivorByIdentity.insert(identity, workExperience);
            } else {
                identifiersToRemove.append(workExperience.workExperienceId);
            }
        }

        for (qint64 workExperienceId : identifiersToRemove) {
            if (!removeWorkExperience(workExperienceId)) {
                return -1;
            }
            ++rowsRemoved;
        }
    }

    // --- Schooling --------------------------------------------------------
    {
        QHash<QString, EducationRecord> survivorByIdentity;
        QList<qint64> identifiersToRemove;

        for (const EducationRecord &educationRecord : loadAllEducationRecords()) {
            const QString identity = educationRecord.schoolName.trimmed().toLower()
                + QStringLiteral("\n") + educationRecord.credentialText.trimmed().toLower()
                + QStringLiteral("\n") + educationRecord.fieldOfStudyText.trimmed().toLower()
                + QStringLiteral("\n") + educationRecord.endDateText.trimmed().toLower();

            if (identity.trimmed().isEmpty()) {
                continue;
            }

            if (!survivorByIdentity.contains(identity)) {
                survivorByIdentity.insert(identity, educationRecord);
                continue;
            }

            const EducationRecord standingSurvivor = survivorByIdentity.value(identity);
            const bool newcomerWins = educationRecord.isConfirmedByUser
                && !standingSurvivor.isConfirmedByUser;
            if (newcomerWins) {
                identifiersToRemove.append(standingSurvivor.educationRecordId);
                survivorByIdentity.insert(identity, educationRecord);
            } else {
                identifiersToRemove.append(educationRecord.educationRecordId);
            }
        }

        for (qint64 educationRecordId : identifiersToRemove) {
            if (!removeEducationRecord(educationRecordId)) {
                return -1;
            }
            ++rowsRemoved;
        }
    }

    return rowsRemoved;
}

QString CareerHistoryRepository::lastErrorText() const
{
    return lastErrorDescription;
}
