#pragma once

#include <QList>
#include <QString>

#include "ActiveJobFolder.h"

class JobCrushDatabase;

// ActiveJobFolderRepository
//
// Reads and writes ActiveJobFolder rows. Callers pass and get back plain
// ActiveJobFolder records. Only this class writes SQL for them.
class ActiveJobFolderRepository {
public:
    explicit ActiveJobFolderRepository(JobCrushDatabase &database);

    // Saves a new link and fills in activeJobFolderId.
    bool insertActiveJobFolder(ActiveJobFolder &activeJobFolder);

    // Every link, in the order they were made.
    QList<ActiveJobFolder> loadAllActiveJobFolders();

    // Replaces every field of an existing link.
    bool updateActiveJobFolder(const ActiveJobFolder &activeJobFolder);

    // Forgets every link. Used when the user picks a different Active Jobs
    // folder: the old folder names mean nothing inside the new one.
    bool removeAllActiveJobFolders();

    // Why the last write or query failed.
    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
