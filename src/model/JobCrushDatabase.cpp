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
            "  discoverySource     TEXT NOT NULL DEFAULT 'manual',"
            "  discoveredTimestamp TEXT NOT NULL"
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

    // The same job must never land twice. A source's own id, paired with the
    // source name, is the only identity Job Crush trusts — titles and company
    // names are written by humans and vary between boards.
    //
    // Partial index: rows with no external id (hand-entered postings) are
    // exempt, because they have no source identity to collide on.
    QSqlQuery uniqueDiscoveryIndexQuery(databaseConnection);
    if (!uniqueDiscoveryIndexQuery.exec(QStringLiteral(
            "CREATE UNIQUE INDEX IF NOT EXISTS uniqueDiscoveryPerSource "
            "ON jobPosting (discoverySource, externalSourceId) "
            "WHERE externalSourceId <> ''"))) {
        lastErrorDescription = uniqueDiscoveryIndexQuery.lastError().text();
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
