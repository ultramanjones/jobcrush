#include "ActiveJobFolderRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// A QString that was never assigned is NULL, and SQLite refuses NULL in a
// NOT NULL column. Same guard as every other repository.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QString::fromLatin1("") : possiblyNullText;
}

QString timestampTextFor(const QDateTime &timestamp)
{
    return timestamp.isValid() ? timestamp.toString(Qt::ISODateWithMs)
                               : QString::fromLatin1("");
}

ActiveJobFolder activeJobFolderFromQueryRow(const QSqlQuery &row)
{
    ActiveJobFolder activeJobFolder;
    activeJobFolder.activeJobFolderId =
        row.value(QStringLiteral("activeJobFolderId")).toLongLong();
    activeJobFolder.folderName = row.value(QStringLiteral("folderName")).toString();
    activeJobFolder.jobApplicationId =
        row.value(QStringLiteral("jobApplicationId")).toLongLong();
    activeJobFolder.fileChangedTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("fileChangedTimestamp")).toString(), Qt::ISODateWithMs);
    activeJobFolder.syncedStageText = row.value(QStringLiteral("syncedStage")).toString();
    activeJobFolder.syncedAppliedTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("syncedAppliedTimestamp")).toString(), Qt::ISODateWithMs);
    activeJobFolder.syncedNotesText = row.value(QStringLiteral("syncedNotesText")).toString();
    return activeJobFolder;
}

void bindEveryField(QSqlQuery &query, const ActiveJobFolder &activeJobFolder)
{
    query.bindValue(QStringLiteral(":folderName"), textOrEmpty(activeJobFolder.folderName));
    query.bindValue(QStringLiteral(":jobApplicationId"), activeJobFolder.jobApplicationId);
    query.bindValue(QStringLiteral(":fileChangedTimestamp"),
                    timestampTextFor(activeJobFolder.fileChangedTimestamp));
    query.bindValue(QStringLiteral(":syncedStage"),
                    textOrEmpty(activeJobFolder.syncedStageText));
    query.bindValue(QStringLiteral(":syncedAppliedTimestamp"),
                    timestampTextFor(activeJobFolder.syncedAppliedTimestamp));
    query.bindValue(QStringLiteral(":syncedNotesText"),
                    textOrEmpty(activeJobFolder.syncedNotesText));
}

} // namespace

ActiveJobFolderRepository::ActiveJobFolderRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool ActiveJobFolderRepository::insertActiveJobFolder(ActiveJobFolder &activeJobFolder)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO activeJobFolder "
        "  (folderName, jobApplicationId, fileChangedTimestamp, syncedStage, "
        "   syncedAppliedTimestamp, syncedNotesText) "
        "VALUES "
        "  (:folderName, :jobApplicationId, :fileChangedTimestamp, :syncedStage, "
        "   :syncedAppliedTimestamp, :syncedNotesText)"));
    bindEveryField(insertQuery, activeJobFolder);

    if (!insertQuery.exec()) {
        lastErrorDescription = insertQuery.lastError().text();
        return false;
    }
    activeJobFolder.activeJobFolderId = insertQuery.lastInsertId().toLongLong();
    return true;
}

QList<ActiveJobFolder> ActiveJobFolderRepository::loadAllActiveJobFolders()
{
    QList<ActiveJobFolder> everyActiveJobFolder;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    if (!selectQuery.exec(QStringLiteral(
            "SELECT * FROM activeJobFolder ORDER BY activeJobFolderId ASC"))) {
        lastErrorDescription = selectQuery.lastError().text();
        return everyActiveJobFolder;
    }
    while (selectQuery.next()) {
        everyActiveJobFolder.append(activeJobFolderFromQueryRow(selectQuery));
    }
    return everyActiveJobFolder;
}

bool ActiveJobFolderRepository::updateActiveJobFolder(const ActiveJobFolder &activeJobFolder)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE activeJobFolder SET "
        "  folderName = :folderName, "
        "  jobApplicationId = :jobApplicationId, "
        "  fileChangedTimestamp = :fileChangedTimestamp, "
        "  syncedStage = :syncedStage, "
        "  syncedAppliedTimestamp = :syncedAppliedTimestamp, "
        "  syncedNotesText = :syncedNotesText "
        "WHERE activeJobFolderId = :activeJobFolderId"));
    bindEveryField(updateQuery, activeJobFolder);
    updateQuery.bindValue(QStringLiteral(":activeJobFolderId"),
                          activeJobFolder.activeJobFolderId);

    if (!updateQuery.exec()) {
        lastErrorDescription = updateQuery.lastError().text();
        return false;
    }
    return true;
}

bool ActiveJobFolderRepository::removeAllActiveJobFolders()
{
    QSqlQuery deleteQuery(jobCrushDatabase.connection());
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM activeJobFolder"))) {
        lastErrorDescription = deleteQuery.lastError().text();
        return false;
    }
    return true;
}

QString ActiveJobFolderRepository::lastErrorText() const
{
    return lastErrorDescription;
}
