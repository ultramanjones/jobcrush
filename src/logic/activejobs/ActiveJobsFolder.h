#pragma once

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "../../model/ActiveJobFolder.h"
#include "JobQuestFile.h"

class ActiveJobFolderRepository;
class JobPipelines;
class JobPostingRepository;
struct TargetedJob;

// ActiveJobsFolder
//
// Keeps the board and the Active Jobs folder the same.
//
// Job Crush cannot see every job board. People find jobs on FlexJobs,
// ZipRecruiter and many other sites, often with an outside AI doing the work.
// The Active Jobs folder is how those jobs reach the board: each job is a
// subfolder holding a JobQuest.txt file, and any AI that can write a text file
// can add a job or change one. Resumes and cover letters go in the same
// subfolder, so the user always knows where to find them.
//
// It works in both directions:
//  - A JobQuest.txt that is new or has changed is read, and the board follows
//    it. Windows says when a file changes; Job Crush waits one second after
//    the last change before reading, so a half-written file is never read.
//    On launch every file's last-changed time is compared with the one saved,
//    to catch changes made while Job Crush was closed.
//  - A card the user moves, dates or writes notes on is written back into its
//    JobQuest.txt. Every card on the board gets a job folder, so this is true
//    for jobs Job Crush found itself as well.
//
// If both change at once, the newest change wins.
//
// What it never does: delete anything. A folder the user deletes is not made
// again, and a card taken off the board leaves its folder where it is.
class ActiveJobsFolder : public QObject {
    Q_OBJECT
public:
    ActiveJobsFolder(JobPipelines &pipelines,
                     JobPostingRepository &postingRepository,
                     ActiveJobFolderRepository &activeJobFolderRepository,
                     QObject *parent = nullptr);

    // Reads which folder the user chose last time. Called once from the
    // composition root.
    void loadFromSettings();

    // Makes the folder if it is not there, reads every job folder in it, gives
    // every card on the board a job folder, and starts watching for changes.
    // Called once from the composition root, after the board is loaded.
    void startKeepingTheBoardAndFolderTheSame();

    // Where the Active Jobs folder is.
    QString folderPath() const;

    // Where it is when the user has not chosen: Documents/Job Crush/Active Jobs.
    static QString defaultFolderPath();

    // Moves Job Crush to a different Active Jobs folder. The old folder is
    // left exactly as it is. The board's cards get job folders in the new
    // one. Returns false with the reason when the folder cannot be used.
    bool setFolderPath(const QString &newFolderPath, QString &reasonText);

    // Brings in job folders from somewhere else: a folder of job folders, or
    // one job folder. Each one is COPIED into the Active Jobs folder, so the
    // originals are never changed. Older folders that have a HOW TO APPLY.txt
    // instead of a JobQuest.txt are read too, and get a JobQuest.txt in the
    // copy. reasonText says what happened in one or two sentences.
    bool importJobFolders(const QString &chosenFolderPath, QString &reasonText);

    // How many job folders the Active Jobs folder holds.
    int jobFolderCount() const;

    // What the last look at the folder found, in one sentence.
    QString lastLookText() const;

    // One sentence per job folder that needs fixing, each starting with the
    // folder's name. Empty when every folder is fine.
    QStringList folderProblemSentences() const;

    // The full path of the job folder for one card, or empty when the card
    // has none.
    QString jobFolderPathForCard(qint64 jobApplicationId) const;

    // The text the user pastes into an outside AI. See AiBuddyInstructions.h.
    QString instructionsForAnOutsideAi() const;

    // Reads every job folder that is new or has changed, and gives every card
    // that has no folder one. The watcher calls this one second after the
    // last change on disk. The tests call it directly.
    void lookAtEveryJobFolder();

signals:
    void folderPathChanged();

    // Something about the folder changed: the count, the problems, or the
    // last-look sentence.
    void folderStatusChanged();

private:
    // The watcher saw a change. Starts the one-second wait again.
    void somethingChangedOnDisk();

    // Writes each card's column, applied date and notes into its JobQuest.txt
    // when they differ from what was last written. Makes a job folder for any
    // card that has none. Runs whenever the board changes.
    void writeBoardChangesToFolders();

    // Points the disk watcher at the Active Jobs folder, every job folder in
    // it, and every JobQuest.txt.
    void watchEveryJobFolder();

    // Reading one file.

    // A known folder's file changed. Moves the card, sets its date and notes,
    // and corrects the job's facts, to match the file.
    void followChangedFile(ActiveJobFolder &link, const JobQuest &quest,
                           const QDateTime &fileChangedTimestamp,
                           const QString &folderName);

    // A folder Job Crush has not seen before. Ties it to the card for the same
    // job if there is one, or puts a new card on the board. False when it
    // could do neither; the problem list or the log says why.
    bool putNewFolderOnTheBoard(const QString &folderName, const JobQuest &quest,
                                const QDateTime &fileChangedTimestamp);

    // Writing one file.

    // Makes a new job folder and JobQuest.txt for a card that has none.
    void makeJobFolderForCard(const TargetedJob &targetedJob);

    // Saves JobQuest.txt in one job folder and hands back the file's new
    // last-changed time. False, with a log line, when the write failed.
    bool writeQuestFile(const QString &folderName, const QString &fileText,
                        QDateTime &fileChangedTimestamp);

    // A folder name Windows accepts and no folder in Active Jobs has yet.
    QString uniqueFolderNameFor(const QString &wantedName) const;

    // The saved link for one folder name or one card, or nullptr.
    ActiveJobFolder *linkForFolder(const QString &folderName);
    ActiveJobFolder *linkForCard(qint64 jobApplicationId);
    const ActiveJobFolder *linkForCard(qint64 jobApplicationId) const;

    // Adds "folder name: sentence" to the list the Settings page shows.
    void reportProblem(const QString &folderName, const QString &problemSentence);

    JobPipelines &board;
    JobPostingRepository &postingRepository;
    ActiveJobFolderRepository &linkRepository;

    // The folder the user picked. Empty means the default folder.
    QString chosenFolderPath;

    // One link per job folder. A link ties a folder name to a card, and holds
    // what was last read from or written to the file, so a change on either
    // side can be spotted.
    QList<ActiveJobFolder> links;
    QFileSystemWatcher diskWatcher;
    QTimer waitForWritingToStop;

    // True while Job Crush is following the files. The board changes as it
    // does, and those changes must not be written straight back.
    bool followingTheFiles = false;

    // False until the composition root calls
    // startKeepingTheBoardAndFolderTheSame. Nothing is read or written before.
    bool hasStarted = false;

    // What the last look found, for the Settings page.
    int jobFoldersSeen = 0;
    QString lastLookSentence;
    QStringList problemSentences;
};
