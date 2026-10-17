#pragma once

#include <QDateTime>
#include <QString>

// ActiveJobFolder
//
// Ties one job folder on disk to one card on the board. Pure data.
//
// Each job in the Active Jobs folder is a subfolder holding a JobQuest.txt
// file. This record says which card that folder belongs to, and what the file
// and the card last agreed on. Job Crush compares against these "synced"
// values to tell which side changed: the file (an outside AI edited it) or the
// card (the user moved it in Job Crush).
struct ActiveJobFolder {
    qint64 activeJobFolderId = 0;       // database identity; 0 means "not saved yet"

    // The subfolder's name inside the Active Jobs folder, for example
    // "Acme Robotics - Senior Qt Developer". Not a full path, so the record
    // still means something if the user moves the whole Active Jobs folder.
    QString folderName;

    // The card this folder belongs to.
    qint64 jobApplicationId = 0;

    // When JobQuest.txt was last changed, as of the last time Job Crush read
    // or wrote it. A file whose time differs has changed since.
    // Invalid means "read it again on the next look", whatever its time says.
    QDateTime fileChangedTimestamp;

    // What the file and the card last agreed on.
    QString syncedStageText;            // the stage's storage word: "applied"
    QDateTime syncedAppliedTimestamp;
    QString syncedNotesText;
};
