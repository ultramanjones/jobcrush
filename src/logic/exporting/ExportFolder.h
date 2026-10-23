#pragma once

#include <QDir>
#include <QStandardPaths>
#include <QString>

#include "../AppPreferences.h"

// ExportFolder
//
// The one place Job Crush writes the files it makes: exported packets, and
// documents converted from one format to another.
//
// It lived inside StagingPacketViewModel until ProDocs needed the same answer.
// Two screens deciding separately where the app's output goes is two screens
// that will eventually disagree, and the user is the one who then cannot find
// their file.
namespace ExportFolder {

// The user's choice when they have made one. Otherwise Documents/Job Crush
// Packets, and if there is no Documents folder at all, a folder beside the
// app's own data.
inline QString folderJobCrushWritesTo(const AppPreferences &preferences,
                                      const QString &applicationDataFolderPath)
{
    const QString chosenFolderPath = preferences.exportFolderPath();
    if (!chosenFolderPath.trimmed().isEmpty()) {
        return chosenFolderPath;
    }

    const QString documentsFolderPath =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documentsFolderPath.isEmpty()) {
        return QDir(applicationDataFolderPath).filePath(QStringLiteral("packets"));
    }
    return QDir(documentsFolderPath).filePath(QStringLiteral("Job Crush Packets"));
}

} // namespace ExportFolder
