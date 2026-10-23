#pragma once

#include <QDir>
#include <QString>

#include "JobQuestFile.h"

// OlderJobFolderReader
//
// Reads a job folder made before JobQuest.txt existed, so those jobs can be
// brought into Job Crush too.
//
// The older layout:
//   - The folder is named "Company - Title".
//   - It holds a file named HOW TO APPLY.txt. Its HOW: line has the link to
//     apply at, its PAY: line has the pay, and its WHERE: line has the place.
//   - Every job in this layout was already sent, so it goes on the board as
//     Applied. The applied date is the folder's last-changed date. That is
//     close enough, and the user can fix any one on the card.
namespace OlderJobFolderReader {

// The file that marks a folder as the older layout.
inline const QString HowToApplyFileName = QStringLiteral("HOW TO APPLY.txt");

// True when this folder is in the older layout: it has HOW TO APPLY.txt and
// no JobQuest.txt.
bool isOlderJobFolder(const QDir &jobFolder);

// What the older folder says, written as a JobQuest. Returns false, with the
// reason in problemSentence, when the folder name has no " - " to split the
// company from the title.
bool readOlderJobFolder(const QDir &jobFolder, JobQuest &questRead,
                        QString &problemSentence);

} // namespace OlderJobFolderReader
