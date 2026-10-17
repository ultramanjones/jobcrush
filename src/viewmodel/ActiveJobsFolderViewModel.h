#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ActiveJobsFolder;

// ActiveJobsFolderViewModel
//
// Serves the Active Jobs folder and AI Buddy Mode to the Settings page: where
// the folder is, what the last look found, which job folders need a fix, and
// the instructions to copy into an outside AI.
class ActiveJobsFolderViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString folderPath READ folderPath NOTIFY folderPathChanged)
    Q_PROPERTY(int jobFolderCount READ jobFolderCount NOTIFY folderStatusChanged)
    Q_PROPERTY(QString lastLookText READ lastLookText NOTIFY folderStatusChanged)
    Q_PROPERTY(QStringList folderProblemSentences READ folderProblemSentences
                   NOTIFY folderStatusChanged)

    // What the last button press did, in one or two sentences. Empty until
    // something is pressed.
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY lastActionChanged)

    // The text the user pastes into an outside AI.
    Q_PROPERTY(QString aiBuddyInstructions READ aiBuddyInstructions NOTIFY folderPathChanged)

public:
    explicit ActiveJobsFolderViewModel(ActiveJobsFolder &activeJobsFolder,
                                       QObject *parent = nullptr);

    QString folderPath() const;
    int jobFolderCount() const;
    QString lastLookText() const;
    QStringList folderProblemSentences() const;
    QString lastActionText() const;
    QString aiBuddyInstructions() const;

    // Opens the Active Jobs folder in the file manager.
    Q_INVOKABLE void openFolder() const;

    // The folder dialog hands back a file:// address. These take it as text.
    Q_INVOKABLE void useFolder(const QString &chosenFolderUrl);
    Q_INVOKABLE void importJobFoldersFrom(const QString &chosenFolderUrl);

    // Puts the AI Buddy Mode instructions on the clipboard.
    Q_INVOKABLE void copyAiBuddyInstructions();

signals:
    void folderPathChanged();
    void folderStatusChanged();
    void lastActionChanged();

private:
    void setLastActionText(const QString &text);

    ActiveJobsFolder &activeJobs;
    QString lastActionSentence;
};
