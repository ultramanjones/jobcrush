#pragma once

#include <QSqlDatabase>
#include <QString>

// JobCrushDatabase
//
// Owns the SQLite connection and the schema. This is the bottom of the stack —
// close to the metal. Nothing above the model layer ever touches SQL directly;
// repositories go through this class, and everything above repositories doesn't
// even know SQLite exists.
//
// Design notes:
//  - Timestamps are stored as ISO 8601 text and the pipeline stage as plain
//    words, so a human opening the database file can read their own data.
//  - The schema is created on first open. Future schema migrations will live
//    here too, versioned by the schemaVersion table.
class JobCrushDatabase {
public:
    JobCrushDatabase() = default;
    ~JobCrushDatabase();

    // Not copyable: there is exactly one database object, wired in the
    // composition root (main.cpp) and handed to repositories by reference.
    JobCrushDatabase(const JobCrushDatabase &) = delete;
    JobCrushDatabase &operator=(const JobCrushDatabase &) = delete;

    // Opens (creating if necessary) the database file and ensures the schema
    // exists. Returns false, with the reason in lastErrorText(), on failure.
    bool openAtFilePath(const QString &databaseFilePath);

    // The live connection, for repositories only.
    QSqlDatabase &connection();

    QString lastErrorText() const;

    bool isOpen() const;

private:
    bool createSchemaIfMissing();

    QSqlDatabase databaseConnection;
    QString lastErrorDescription;
};
