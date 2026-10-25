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
//  - The schema is checked on every open. Missing tables are made, and
//    columns added since are put into older files in place, so no rows are
//    lost. The schemaVersion table holds 1 and is not yet used to choose
//    what to change.
class JobCrushDatabase {
public:
    JobCrushDatabase() = default;
    ~JobCrushDatabase();

    // Not copyable: there is exactly one database object, wired in the
    // composition root (main.cpp) and handed to repositories by reference.
    JobCrushDatabase(const JobCrushDatabase &) = delete;
    JobCrushDatabase &operator=(const JobCrushDatabase &) = delete;

    // Opens the database file, making it if needed, and makes any missing
    // part of the schema. Returns false, with the reason in lastErrorText(),
    // on failure.
    bool openAtFilePath(const QString &databaseFilePath);

    // Opens a second connection to a database file that openAtFilePath has
    // already set up, under its own connection name. Used by the background
    // thread that saves and ranks jobs: Qt lets a connection be used only on
    // the thread that opened it, so that thread needs its own. It does not
    // create or change the schema. Must be called on the thread that will
    // use it.
    bool openExtraConnection(const QString &databaseFilePath, const QString &connectionName);

    // The live connection, for repositories only.
    QSqlDatabase &connection();

    // Why the last open or schema step failed.
    QString lastErrorText() const;

    bool isOpen() const;

private:
    // Makes every table and index that is missing, and adds or renames the
    // columns that changed since older builds. Safe to run on every open.
    bool createSchemaIfMissing();

    // Sets this connection's sync mode to NORMAL. Used only on the
    // background connection. See the .cpp for why.
    void useNormalSyncMode();

    // Adds a column to an existing table only if it is not already there.
    // This is how the schema grows without throwing away anyone's data: a
    // database created before JobScout existed gains the new columns on the
    // next launch and keeps every row it had.
    bool addColumnIfMissing(const QString &tableName,
                            const QString &columnName,
                            const QString &columnDefinition);

    // Renames a column that is still under its old name, and does nothing at
    // all once the new name is in place. Needed because CREATE TABLE IF NOT
    // EXISTS cannot fix a table that already exists: a database written before
    // a rename keeps the old column forever unless something moves it.
    bool renameColumnIfNeeded(const QString &tableName,
                              const QString &oldColumnName,
                              const QString &newColumnName);

    // Answers whether a table already has a column of this name.
    bool tableHasColumn(const QString &tableName, const QString &columnName);

    QSqlDatabase databaseConnection;
    QString lastErrorDescription;
};
