#include "ActiveJobsFolderViewModel.h"

#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QUrl>

#include "../logic/activejobs/ActiveJobsFolder.h"

namespace {

// The folder dialog hands back "file:///C:/Users/...". Anything else is
// treated as a plain path, so a typed path works too.
QString localPathFrom(const QString &chosenFolderUrl)
{
    const QUrl chosenUrl(chosenFolderUrl);
    return chosenUrl.isLocalFile() ? chosenUrl.toLocalFile() : chosenFolderUrl;
}

} // namespace

ActiveJobsFolderViewModel::ActiveJobsFolderViewModel(ActiveJobsFolder &activeJobsFolder,
                                                     QObject *parent)
    : QObject(parent)
    , activeJobs(activeJobsFolder)
{
    connect(&activeJobs, &ActiveJobsFolder::folderPathChanged,
            this, &ActiveJobsFolderViewModel::folderPathChanged);
    connect(&activeJobs, &ActiveJobsFolder::folderStatusChanged,
            this, &ActiveJobsFolderViewModel::folderStatusChanged);
}

QString ActiveJobsFolderViewModel::folderPath() const
{
    return QDir::toNativeSeparators(activeJobs.folderPath());
}

int ActiveJobsFolderViewModel::jobFolderCount() const
{
    return activeJobs.jobFolderCount();
}

QString ActiveJobsFolderViewModel::lastLookText() const
{
    return activeJobs.lastLookText();
}

QStringList ActiveJobsFolderViewModel::folderProblemSentences() const
{
    return activeJobs.folderProblemSentences();
}

QString ActiveJobsFolderViewModel::lastActionText() const
{
    return lastActionSentence;
}

QString ActiveJobsFolderViewModel::aiBuddyInstructions() const
{
    return activeJobs.instructionsForAnOutsideAi();
}

void ActiveJobsFolderViewModel::openFolder() const
{
    const QString folderPathToOpen = activeJobs.folderPath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPathToOpen))) {
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolderViewModel::openFolder: could not open \"%1\" — the system "
            "file manager refused it").arg(folderPathToOpen);
    }
}

void ActiveJobsFolderViewModel::useFolder(const QString &chosenFolderUrl)
{
    QString reasonText;
    activeJobs.setFolderPath(localPathFrom(chosenFolderUrl), reasonText);
    setLastActionText(reasonText);
}

void ActiveJobsFolderViewModel::importJobFoldersFrom(const QString &chosenFolderUrl)
{
    QString reasonText;
    activeJobs.importJobFolders(localPathFrom(chosenFolderUrl), reasonText);
    setLastActionText(reasonText);
}

void ActiveJobsFolderViewModel::copyAiBuddyInstructions()
{
    QGuiApplication::clipboard()->setText(activeJobs.instructionsForAnOutsideAi());
    setLastActionText(QStringLiteral("Copied. Paste it into your AI once, and it will "
                                     "keep your board up to date."));
}

void ActiveJobsFolderViewModel::setLastActionText(const QString &text)
{
    lastActionSentence = text;
    emit lastActionChanged();
}
