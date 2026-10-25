#include "JobCrushDatabase.h"

#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>

namespace {
// Each connection has a name. The background thread that saves and ranks
// jobs opens a second one under its own name.
const QString primaryConnectionName = QStringLiteral("jobCrushPrimaryConnection");
} // namespace

JobCrushDatabase::~JobCrushDatabase()
{
    const QString connectionName = databaseConnection.connectionName();
    if (databaseConnection.isOpen()) {
        databaseConnection.close();
    }
    // Let go of the connection's name too, so the same name can be opened
    // again later. Qt wants every copy of the handle gone first.
    databaseConnection = QSqlDatabase();
    if (!connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(connectionName);
    }
}

bool JobCrushDatabase::openAtFilePath(const QString &databaseFilePath)
{
    // Make sure the folder for the database file exists before SQLite tries
    // to create the file inside it.
    const QFileInfo databaseFileInfo(databaseFilePath);
    const QDir databaseFolder = databaseFileInfo.absoluteDir();
    if (!databaseFolder.exists() && !databaseFolder.mkpath(QStringLiteral("."))) {
        lastErrorDescription =
            QStringLiteral("Could not create folder: %1").arg(databaseFolder.absolutePath());
        return false;
    }

    databaseConnection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), primaryConnectionName);
    databaseConnection.setDatabaseName(databaseFilePath);

    if (!databaseConnection.open()) {
        lastErrorDescription = databaseConnection.lastError().text();
        return false;
    }

    // Enforce foreign keys — SQLite leaves them off by default.
    QSqlQuery enableForeignKeysQuery(databaseConnection);
    if (!enableForeignKeysQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        lastErrorDescription = enableForeignKeysQuery.lastError().text();
        return false;
    }

    // WAL mode lets the screen's thread read while the background thread
    // writes. Without it, a read waits until a write is done, and the window
    // stops responding while it waits. SQLite keeps this setting in the file.
    // If it cannot be turned on, Job Crush still works, only slower to
    // answer during a sweep, so this is logged and not treated as fatal.
    QSqlQuery walModeQuery(databaseConnection);
    if (!walModeQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        qWarning().noquote() << QStringLiteral(
            "JobCrushDatabase::openAtFilePath: could not turn on WAL mode for %1 — %2")
                .arg(databaseFilePath, walModeQuery.lastError().text());
    }

    return createSchemaIfMissing();
}

bool JobCrushDatabase::openExtraConnection(const QString &databaseFilePath,
                                           const QString &connectionName)
{
    databaseConnection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    databaseConnection.setDatabaseName(databaseFilePath);

    if (!databaseConnection.open()) {
        lastErrorDescription = databaseConnection.lastError().text();
        return false;
    }

    QSqlQuery enableForeignKeysQuery(databaseConnection);
    if (!enableForeignKeysQuery.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        lastErrorDescription = enableForeignKeysQuery.lastError().text();
        return false;
    }
    useNormalSyncMode();
    return true;
}

void JobCrushDatabase::useNormalSyncMode()
{
    // Used only on the background connection, which saves found jobs. In
    // WAL mode, NORMAL makes the disk safe at each checkpoint instead of at
    // every commit, so a sweep that saves one company at a time does not wait
    // on the disk hundreds of times. The database cannot be damaged this way.
    // If the power fails, the last few saved finds may be lost, and the next
    // sweep finds those jobs again.
    //
    // The main connection keeps SQLite's default, FULL, because it saves what
    // the user does by hand: notes, dates, cards. Those must never be lost.
    QSqlQuery syncModeQuery(databaseConnection);
    if (!syncModeQuery.exec(QStringLiteral("PRAGMA synchronous = NORMAL"))) {
        qWarning().noquote() << QStringLiteral(
            "JobCrushDatabase::useNormalSyncMode: could not set it on connection %1 — %2")
                .arg(databaseConnection.connectionName(), syncModeQuery.lastError().text());
    }
}

QSqlDatabase &JobCrushDatabase::connection()
{
    return databaseConnection;
}

QString JobCrushDatabase::lastErrorText() const
{
    return lastErrorDescription;
}

bool JobCrushDatabase::isOpen() const
{
    return databaseConnection.isOpen();
}

bool JobCrushDatabase::createSchemaIfMissing()
{
    // Each statement says IF NOT EXISTS, so it does nothing when the table is
    // already there. Running this on every startup is safe and cheap.
    const QStringList schemaStatements = {
        // Schema version bookkeeping, for future migrations.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schemaVersion ("
            "  versionNumber INTEGER NOT NULL"
            ")"),

        // A job that exists out in the world (see JobPosting.h).
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS jobPosting ("
            "  jobPostingId        INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  companyName         TEXT NOT NULL,"
            "  positionTitle       TEXT NOT NULL,"
            "  locationText        TEXT NOT NULL DEFAULT '',"
            "  salaryText          TEXT NOT NULL DEFAULT '',"
            "  sourceUrl           TEXT NOT NULL DEFAULT '',"
            "  fullDescriptionText TEXT NOT NULL DEFAULT '',"
            "  postingSource       TEXT NOT NULL DEFAULT '',"
            "  scoutSource         TEXT NOT NULL DEFAULT 'scout',"
            "  discoveredTimestamp TEXT NOT NULL"
            ")"),

        // Every place one job can be read or applied to (see PostingSource.h).
        // A job is usually reachable in more than one, and the user picks.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS jobPostingSource ("
            "  postingSourceId INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  jobPostingId    INTEGER NOT NULL REFERENCES jobPosting(jobPostingId),"
            "  boardName       TEXT NOT NULL,"
            "  externalId      TEXT NOT NULL DEFAULT '',"
            "  postingUrl      TEXT NOT NULL"
            ")"),

        // The user's campaign for one posting (see JobApplication.h).
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS jobApplication ("
            "  jobApplicationId  INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  jobPostingId      INTEGER NOT NULL REFERENCES jobPosting(jobPostingId),"
            "  pipelineStage     TEXT NOT NULL DEFAULT 'saved',"
            "  targetedTimestamp TEXT NOT NULL,"
            "  appliedTimestamp  TEXT NOT NULL DEFAULT '',"
            "  notesText         TEXT NOT NULL DEFAULT ''"
            ")"),

        // One job the user has held (see WorkExperience.h). Dates are text
        // because that is how resumes write them.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS workExperience ("
            "  workExperienceId  INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  employerName      TEXT NOT NULL DEFAULT '',"
            "  roleTitle         TEXT NOT NULL DEFAULT '',"
            "  startDateText     TEXT NOT NULL DEFAULT '',"
            "  endDateText       TEXT NOT NULL DEFAULT '',"
            "  summaryText       TEXT NOT NULL DEFAULT '',"
            "  sourceDocumentId  INTEGER NOT NULL DEFAULT 0,"
            "  sourceLineText    TEXT NOT NULL DEFAULT '',"
            "  isConfirmedByUser INTEGER NOT NULL DEFAULT 0"
            ")"),

        // One school or credential (see EducationRecord.h).
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS educationRecord ("
            "  educationRecordId INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  schoolName        TEXT NOT NULL DEFAULT '',"
            "  credentialText    TEXT NOT NULL DEFAULT '',"
            "  fieldOfStudyText  TEXT NOT NULL DEFAULT '',"
            "  startDateText     TEXT NOT NULL DEFAULT '',"
            "  endDateText       TEXT NOT NULL DEFAULT '',"
            "  sourceDocumentId  INTEGER NOT NULL DEFAULT 0,"
            "  sourceLineText    TEXT NOT NULL DEFAULT '',"
            "  isConfirmedByUser INTEGER NOT NULL DEFAULT 0"
            ")"),

        // One piece of one application packet (see StagedDocument.h).
        // There is no foreign key here, so deleting a campaign does not
        // delete its packet. StagedDocumentRepository::removePacketForApplication
        // does that, when it is called.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS stagedDocument ("
            "  stagedDocumentId    INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  jobApplicationId    INTEGER NOT NULL,"
            "  documentKind        TEXT NOT NULL DEFAULT 'other',"
            "  titleText           TEXT NOT NULL DEFAULT '',"
            "  markdownText        TEXT NOT NULL DEFAULT '',"
            "  wasWrittenByBrain   INTEGER NOT NULL DEFAULT 0,"
            "  wasEditedByUser     INTEGER NOT NULL DEFAULT 0,"
            "  isApprovedByUser    INTEGER NOT NULL DEFAULT 0,"
            "  createdTimestamp    TEXT NOT NULL DEFAULT '',"
            "  lastEditedTimestamp TEXT NOT NULL DEFAULT ''"
            ")"),

        // One job folder in the Active Jobs folder, tied to one card on the
        // board (see ActiveJobFolder.h). Folder names are compared without
        // case, the way Windows compares them.
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS activeJobFolder ("
            "  activeJobFolderId      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  folderName             TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "  jobApplicationId       INTEGER NOT NULL,"
            "  fileChangedTimestamp   TEXT NOT NULL DEFAULT '',"
            "  syncedStage            TEXT NOT NULL DEFAULT '',"
            "  syncedAppliedTimestamp TEXT NOT NULL DEFAULT '',"
            "  syncedNotesText        TEXT NOT NULL DEFAULT ''"
            ")"),

        // One ProDocs item (see ProfessionalDocument.h).
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS professionalDocument ("
            "  professionalDocumentId INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  documentKind           TEXT NOT NULL DEFAULT 'other',"
            "  displayName            TEXT NOT NULL,"
            "  originalFilePath       TEXT NOT NULL DEFAULT '',"
            "  extractedText          TEXT NOT NULL DEFAULT '',"
            "  importedTimestamp      TEXT NOT NULL"
            ")"),
    };

    for (const QString &statementText : schemaStatements) {
        QSqlQuery schemaQuery(databaseConnection);
        if (!schemaQuery.exec(statementText)) {
            lastErrorDescription = schemaQuery.lastError().text();
            return false;
        }
    }

    // --- Schema growth: the columns JobScout added to jobPosting ----------
    //
    // A database created before JobScout existed is missing these. Adding
    // them here, one at a time and only when absent, means an existing user
    // keeps every row they had instead of starting over.
    if (!addColumnIfMissing(QStringLiteral("jobPosting"),
                            QStringLiteral("externalSourceId"),
                            QStringLiteral("TEXT NOT NULL DEFAULT ''"))
        || !addColumnIfMissing(QStringLiteral("jobPosting"),
                               QStringLiteral("postedTimestamp"),
                               QStringLiteral("TEXT NOT NULL DEFAULT ''"))
        || !addColumnIfMissing(QStringLiteral("jobPosting"),
                               QStringLiteral("isRemoteRole"),
                               QStringLiteral("INTEGER NOT NULL DEFAULT 0"))) {
        return false;
    }

    // --- Schema growth: the columns ProDocs added ------------------------
    if (!addColumnIfMissing(QStringLiteral("professionalDocument"),
                            QStringLiteral("storedFilePath"),
                            QStringLiteral("TEXT NOT NULL DEFAULT ''"))
        || !addColumnIfMissing(QStringLiteral("professionalDocument"),
                               QStringLiteral("fileSizeBytes"),
                               QStringLiteral("INTEGER NOT NULL DEFAULT 0"))
        || !addColumnIfMissing(QStringLiteral("professionalDocument"),
                               QStringLiteral("textExtractionNote"),
                               QStringLiteral("TEXT NOT NULL DEFAULT ''"))
        || !addColumnIfMissing(QStringLiteral("professionalDocument"),
                               QStringLiteral("hasBeenReadForInsights"),
                               QStringLiteral("INTEGER NOT NULL DEFAULT 0"))) {
        return false;
    }

    // Did a PERSON touch this entry, or is it still exactly what the reader
    // produced? The difference decides what a better reader is allowed to
    // throw away and re-do. Defaults to 0, which is the truth for every row
    // that existed before the column did: they were read, not written.
    if (!addColumnIfMissing(QStringLiteral("workExperience"),
                            QStringLiteral("wasEditedByUser"),
                            QStringLiteral("INTEGER NOT NULL DEFAULT 0"))
        || !addColumnIfMissing(QStringLiteral("educationRecord"),
                               QStringLiteral("wasEditedByUser"),
                               QStringLiteral("INTEGER NOT NULL DEFAULT 0"))) {
        return false;
    }

    // --- Schema growth: the fit score on each campaign --------------------
    //
    // Defaults to -1, not 0. Unscored and scored-zero are different, and every
    // row that existed before scoring existed is unscored.
    if (!addColumnIfMissing(QStringLiteral("jobApplication"),
                            QStringLiteral("fitScorePercent"),
                            QStringLiteral("INTEGER NOT NULL DEFAULT -1"))) {
        return false;
    }

    // Packets are only ever looked up by campaign.
    QSqlQuery packetIndexQuery(databaseConnection);
    if (!packetIndexQuery.exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS stagedDocumentByApplication "
            "ON stagedDocument (jobApplicationId)"))) {
        lastErrorDescription = packetIndexQuery.lastError().text();
        return false;
    }

    // discoverySource was always the wrong name: it held the BOARD the posting
    // was read off, never who found it. Move it to postingSource, and give
    // every database written before this the same two columns a fresh one gets.
    //
    // Order matters. The rename has to happen before the unique index below is
    // built on the new name, and the old index has to go first because it
    // still names the old column.
    if (tableHasColumn(QStringLiteral("jobPosting"), QStringLiteral("discoverySource"))
            && !tableHasColumn(QStringLiteral("jobPosting"),
                               QStringLiteral("postingSource"))) {
        QSqlQuery dropOldIndexQuery(databaseConnection);
        if (!dropOldIndexQuery.exec(QStringLiteral(
                "DROP INDEX IF EXISTS uniqueDiscoveryPerSource"))) {
            lastErrorDescription = dropOldIndexQuery.lastError().text();
            return false;
        }
        if (!renameColumnIfNeeded(QStringLiteral("jobPosting"),
                                  QStringLiteral("discoverySource"),
                                  QStringLiteral("postingSource"))) {
            return false;
        }

        // The old column said 'manual' for a hand-entered posting. That was
        // an answer to the other question, so it does not belong in
        // postingSource: a hand-entered job was read off no board at all.
        QSqlQuery clearManualQuery(databaseConnection);
        if (!clearManualQuery.exec(QStringLiteral(
                "UPDATE jobPosting SET postingSource = '' "
                "WHERE postingSource = 'manual'"))) {
            lastErrorDescription = clearManualQuery.lastError().text();
            return false;
        }
    }

    // Who brought the job in. Older rows all default to 'scout': the app had
    // no way to tell a hand-added job from a swept one before this column
    // existed, and claiming otherwise would be inventing history.
    if (!addColumnIfMissing(QStringLiteral("jobPosting"),
                            QStringLiteral("scoutSource"),
                            QStringLiteral("TEXT NOT NULL DEFAULT 'scout'"))) {
        return false;
    }

    // When the posting was last found missing from the employer's board.
    // Empty means it was still there, or has never been checked.
    if (!addColumnIfMissing(QStringLiteral("jobPosting"),
                            QStringLiteral("postingGoneSinceTimestamp"),
                            QStringLiteral("TEXT NOT NULL DEFAULT ''"))) {
        return false;
    }

    // When the user said the job is still open after checking it. Empty means
    // they never did.
    if (!addColumnIfMissing(QStringLiteral("jobPosting"),
                            QStringLiteral("userSaysStillOpenTimestamp"),
                            QStringLiteral("TEXT NOT NULL DEFAULT ''"))) {
        return false;
    }

    // The same job must never land twice. A board's own id, paired with the
    // board name, is the only identity Job Crush trusts — titles and company
    // names are written by humans and vary between boards.
    //
    // Partial index: rows with no external id (hand-entered postings) are
    // exempt, because they have no board identity to collide on.
    QSqlQuery uniquePostingIndexQuery(databaseConnection);
    if (!uniquePostingIndexQuery.exec(QStringLiteral(
            "CREATE UNIQUE INDEX IF NOT EXISTS uniquePostingPerBoard "
            "ON jobPosting (postingSource, externalSourceId) "
            "WHERE externalSourceId <> ''"))) {
        lastErrorDescription = uniquePostingIndexQuery.lastError().text();
        return false;
    }

    // One route per board per job. Two rows for the same job on Ashby would
    // put the same link on the card twice.
    QSqlQuery uniqueRouteIndexQuery(databaseConnection);
    if (!uniqueRouteIndexQuery.exec(QStringLiteral(
            "CREATE UNIQUE INDEX IF NOT EXISTS uniqueRoutePerBoard "
            "ON jobPostingSource (jobPostingId, boardName)"))) {
        lastErrorDescription = uniqueRouteIndexQuery.lastError().text();
        return false;
    }

    // Record version 1 exactly once. A failure here is logged and not
    // treated as fatal: nothing reads the version yet.
    QSqlQuery versionQuery(databaseConnection);
    if (!versionQuery.exec(QStringLiteral("SELECT COUNT(*) FROM schemaVersion"))) {
        qWarning().noquote() << QStringLiteral(
            "JobCrushDatabase::createSchemaIfMissing: could not count the rows in "
            "schemaVersion for %1 — %2")
                .arg(databaseConnection.databaseName(), versionQuery.lastError().text());
    }
    if (versionQuery.next() && versionQuery.value(0).toInt() == 0) {
        QSqlQuery insertVersionQuery(databaseConnection);
        if (!insertVersionQuery.exec(
                QStringLiteral("INSERT INTO schemaVersion (versionNumber) VALUES (1)"))) {
            qWarning().noquote() << QStringLiteral(
                "JobCrushDatabase::createSchemaIfMissing: could not record schema "
                "version 1 for %1 — %2")
                    .arg(databaseConnection.databaseName(),
                         insertVersionQuery.lastError().text());
        }
    }

    return true;
}

bool JobCrushDatabase::addColumnIfMissing(const QString &tableName,
                                          const QString &columnName,
                                          const QString &columnDefinition)
{
    // PRAGMA table_info is SQLite's own answer to "what columns does this
    // table have?" It is simpler and safer than reading CREATE statements.
    QSqlQuery existingColumnsQuery(databaseConnection);
    if (!existingColumnsQuery.exec(
            QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        lastErrorDescription = existingColumnsQuery.lastError().text();
        return false;
    }

    while (existingColumnsQuery.next()) {
        // Column 1 of table_info is the column's name.
        if (existingColumnsQuery.value(1).toString() == columnName) {
            return true; // already there; nothing to do
        }
    }

    QSqlQuery addColumnQuery(databaseConnection);
    if (!addColumnQuery.exec(QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                                 .arg(tableName, columnName, columnDefinition))) {
        lastErrorDescription = addColumnQuery.lastError().text();
        return false;
    }
    return true;
}

bool JobCrushDatabase::tableHasColumn(const QString &tableName,
                                      const QString &columnName)
{
    QSqlQuery existingColumnsQuery(databaseConnection);
    if (!existingColumnsQuery.exec(
            QStringLiteral("PRAGMA table_info(%1)").arg(tableName))) {
        // Answers "no", which makes the caller skip the rename. Logged so
        // a skipped rename is not a mystery.
        qWarning().noquote() << QStringLiteral(
            "JobCrushDatabase::tableHasColumn: could not read the columns of table "
            "%1 — %2").arg(tableName, existingColumnsQuery.lastError().text());
        return false;
    }
    while (existingColumnsQuery.next()) {
        // Column 1 of table_info is the column's name.
        if (existingColumnsQuery.value(1).toString() == columnName) {
            return true;
        }
    }
    return false;
}

bool JobCrushDatabase::renameColumnIfNeeded(const QString &tableName,
                                            const QString &oldColumnName,
                                            const QString &newColumnName)
{
    if (tableHasColumn(tableName, newColumnName)) {
        return true; // already moved; nothing to do
    }
    if (!tableHasColumn(tableName, oldColumnName)) {
        return true; // nothing to move; a fresh database is already right
    }

    QSqlQuery renameQuery(databaseConnection);
    if (!renameQuery.exec(QStringLiteral("ALTER TABLE %1 RENAME COLUMN %2 TO %3")
                              .arg(tableName, oldColumnName, newColumnName))) {
        lastErrorDescription = renameQuery.lastError().text();
        return false;
    }
    return true;
}
