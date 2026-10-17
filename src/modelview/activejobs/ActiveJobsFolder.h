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
    void somethingChangedOnDisk();
    void writeBoardChangesToFolders();
    void watchEveryJobFolder();

    // Reading one file.
    void followChangedFile(ActiveJobFolder &link, const JobQuest &quest,
                           const QDateTime &fileChangedTimestamp,
                           const QString &folderName);
    bool putNewFolderOnTheBoard(const QString &folderName, const JobQuest &quest,
                                const QDateTime &fileChangedTimestamp);

    // Writing one file.
    void makeJobFolderForCard(const TargetedJob &targetedJob);
    bool writeQuestFile(const QString &folderName, const QString &fileText,
                        QDateTime &fileChangedTimestamp);

    QString uniqueFolderNameFor(const QString &wantedName) const;
    ActiveJobFolder *linkForFolder(const QString &folderName);
    ActiveJobFolder *linkForCard(qint64 jobApplicationId);
    const ActiveJobFolder *linkForCard(qint64 jobApplicationId) const;
    void reportProblem(const QString &folderName, const QString &problemSentence);

    JobPipelines &board;
    JobPostingRepository &postingRepository;
    ActiveJobFolderRepository &linkRepository;

    QString chosenFolderPath;
    QList<ActiveJobFolder> links;
    QFileSystemWatcher diskWatcher;
    QTimer waitForWritingToStop;

    // True while Job Crush is following the files. The board changes as it
    // does, and those changes must not be written straight back.
    bool followingTheFiles = false;
    bool hasStarted = false;

    int jobFoldersSeen = 0;
    QString lastLookSentence;
    QStringList problemSentences;
};
