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

    // Record version 1 exactly once.
    QSqlQuery versionQuery(databaseConnection);
    versionQuery.exec(QStringLiteral("SELECT COUNT(*) FROM schemaVersion"));
    if (versionQuery.next() && versionQuery.value(0).toInt() == 0) {
        QSqlQuery insertVersionQuery(databaseConnection);
        insertVersionQuery.exec(QStringLiteral("INSERT INTO schemaVersion (versionNumber) VALUES (1)"));
    }

    return true;
}
