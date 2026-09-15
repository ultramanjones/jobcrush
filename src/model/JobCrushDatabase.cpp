#include "JobCrushDatabase.h"

#include <QFileInfo>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>

namespace {
// A named connection keeps us honest if a second connection ever appears
// (for example, a background thread for JobScout imports).
const QString primaryConnectionName = QStringLiteral("jobCrushPrimaryConnection");
} // namespace

JobCrushDatabase::~JobCrushDatabase()
{
    if (databaseConnection.isOpen()) {
        databaseConnection.close();
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

    return createSchemaIfMissing();
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
    // Each statement is idempotent (IF NOT EXISTS), so calling this on every
    // startup is safe and cheap.
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
        // Deleting a campaign also deletes its packet.
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

    // --- Schema growth: the column the task layer added -------------------
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

    // The same job must never land twice. A board's own id, paired with the
    // board name, is the only identity Job Crush trusts — titles and company
    // names are written by humans and vary between boards.
    //
    // Partial index: rows with no external id (hand-entered postings) are
    // exempt, because they have no board identity to collide on.
    QSqlQuery uniqueDiscoveryIndexQuery(databaseConnection);
    if (!uniqueDiscoveryIndexQuery.exec(QStringLiteral(
            "CREATE UNIQUE INDEX IF NOT EXISTS uniquePostingPerBoard "
            "ON jobPosting (postingSource, externalSourceId) "
            "WHERE externalSourceId <> ''"))) {
        lastErrorDescription = uniqueDiscoveryIndexQuery.lastError().text();
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

    // Record version 1 exactly once.
    QSqlQuery versionQuery(databaseConnection);
    versionQuery.exec(QStringLiteral("SELECT COUNT(*) FROM schemaVersion"));
    if (versionQuery.next() && versionQuery.value(0).toInt() == 0) {
        QSqlQuery insertVersionQuery(databaseConnection);
        insertVersionQuery.exec(QStringLiteral("INSERT INTO schemaVersion (versionNumber) VALUES (1)"));
    }

    return true;
}

bool JobCrushDatabase::addColumnIfMissing(const QString &tableName,
                                          const QString &columnName,
                                          const QString &columnDefinition)
{
    // PRAGMA table_info is SQLite's own answer to "what columns does this
    // table have?" — cheaper and more honest than parsing CREATE statements.
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
