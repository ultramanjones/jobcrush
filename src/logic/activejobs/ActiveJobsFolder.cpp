#include "ActiveJobsFolder.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include "../../model/ActiveJobFolderRepository.h"
#include "../../model/JobPostingRepository.h"
#include "../jobscout/DuplicateJobDetector.h"
#include "../pipelines/JobPipelines.h"
#include "AiBuddyInstructions.h"
#include "OlderJobFolderReader.h"

namespace {

const QString folderPathSettingKey = QStringLiteral("activeJobs/folderPath");

// How long to wait after the last change on disk before reading. An AI or an
// editor often saves a file in several steps, and a file read halfway through
// is a file read wrong.
constexpr int quietMillisecondsBeforeReading = 1000;

// The longest name a new job folder gets. A long folder name makes every path
// inside it long, and many Windows programs refuse a path over 260 characters.
constexpr int longestFolderNameLength = 80;

// The board stores an applied date at noon. JobPipelines::setAppliedDate says
// why. The same hour is used here so the two always agree.
constexpr int appliedHourOfDay = 12;

// A name Windows accepts for a folder. The characters it refuses are taken
// out, and so are trailing dots and spaces, which it quietly drops.
QString folderNameWindowsAccepts(const QString &wantedName)
{
    // The characters Windows refuses in a name: < > : " / \ | ? * and the
    // control characters 0 to 31. Each one becomes a space.
    static const QRegularExpression refusedCharacters(
        QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]"));
    QString cleanName = wantedName;
    cleanName.replace(refusedCharacters, QStringLiteral(" "));
    // Runs of spaces become one space, and the ends are trimmed.
    cleanName = cleanName.simplified();
    if (cleanName.size() > longestFolderNameLength) {
        cleanName = cleanName.left(longestFolderNameLength).trimmed();
    }
    while (cleanName.endsWith(QLatin1Char('.')) || cleanName.endsWith(QLatin1Char(' '))) {
        cleanName.chop(1);
    }
    if (cleanName.isEmpty()) {
        cleanName = QStringLiteral("Job");
    }
    return cleanName;
}

// The whole file as text. fileWasRead is false when it could not be opened.
// The caller says what went wrong, because only it knows which folder this is.
QString readWholeFile(const QString &filePath, bool &fileWasRead)
{
    QFile file(filePath);
    fileWasRead = file.open(QIODevice::ReadOnly);
    if (!fileWasRead) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QDateTime lastChangedTimeOf(const QString &filePath)
{
    // A fresh QFileInfo every time. QFileInfo remembers what it read, and a
    // remembered time is how a change gets missed.
    return QFileInfo(filePath).lastModified();
}

// Who made a job, as Job Crush stores it. "Job Crush" and "You" map to the
// two words Job Crush already uses; anything else is kept as written, so a
// job ChatGPT made says ChatGPT.
QString scoutSourceFor(const QString &questCreatedBy)
{
    const QString cleanName = questCreatedBy.simplified();
    const QString lowerName = cleanName.toLower();
    if (cleanName.isEmpty()) {
        return QStringLiteral("Imported");
    }
    if (lowerName == QStringLiteral("job crush") || lowerName == QStringLiteral("jobcrush")) {
        return ScoutSourceText::Scout;
    }
    if (lowerName == QStringLiteral("you") || lowerName == QStringLiteral("me")
            || lowerName == QStringLiteral("myself")) {
        return ScoutSourceText::You;
    }
    return cleanName;
}

// The day only. The board stores an applied date at noon, and comparing whole
// timestamps would call noon and midnight on the same day a change.
QDate appliedDayOf(const QDateTime &appliedTimestamp)
{
    return appliedTimestamp.isValid() ? appliedTimestamp.date() : QDate();
}

// True when a folder holds a job: a JobQuest.txt, or the older HOW TO APPLY.txt.
bool isJobFolder(const QDir &folder)
{
    return folder.exists(JobQuestFile::FileName)
           || folder.exists(OlderJobFolderReader::HowToApplyFileName);
}

// Copies a folder and everything in it. Never overwrites: the target is
// always a folder that did not exist a moment ago.
bool copyFolder(const QString &sourceFolderPath, const QString &targetFolderPath)
{
    QDir sourceFolder(sourceFolderPath);
    if (!QDir().mkpath(targetFolderPath)) {
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolder copyFolder: could not make folder \"%1\" — mkpath failed")
                .arg(targetFolderPath);
        return false;
    }
    // Walks every file and folder below the source, at any depth. Each one
    // goes to the same place below the target. A folder is made before the
    // files in it, because the walk reaches a folder before its contents.
    QDirIterator everyEntry(sourceFolderPath,
                            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                            QDirIterator::Subdirectories);
    while (everyEntry.hasNext()) {
        const QString sourcePath = everyEntry.next();
        const QString relativePath = sourceFolder.relativeFilePath(sourcePath);
        const QString targetPath = QDir(targetFolderPath).filePath(relativePath);
        if (everyEntry.fileInfo().isDir()) {
            if (!QDir().mkpath(targetPath)) {
                qWarning().noquote() << QStringLiteral(
                    "ActiveJobsFolder copyFolder: could not make folder \"%1\" — mkpath "
                    "failed").arg(targetPath);
                return false;
            }
        } else if (!QFile::copy(sourcePath, targetPath)) {
            qWarning().noquote() << QStringLiteral(
                "ActiveJobsFolder copyFolder: could not copy \"%1\" to \"%2\" — QFile::copy "
                "failed").arg(sourcePath, targetPath);
            return false;
        }
    }
    return true;
}

} // namespace

ActiveJobsFolder::ActiveJobsFolder(JobPipelines &pipelines,
                                   JobPostingRepository &postingRepositoryToUse,
                                   ActiveJobFolderRepository &activeJobFolderRepository,
                                   QObject *parent)
    : QObject(parent)
    , board(pipelines)
    , postingRepository(postingRepositoryToUse)
    , linkRepository(activeJobFolderRepository)
{
    waitForWritingToStop.setSingleShot(true);
    waitForWritingToStop.setInterval(quietMillisecondsBeforeReading);
    connect(&waitForWritingToStop, &QTimer::timeout,
            this, &ActiveJobsFolder::lookAtEveryJobFolder);

    connect(&diskWatcher, &QFileSystemWatcher::directoryChanged,
            this, &ActiveJobsFolder::somethingChangedOnDisk);
    connect(&diskWatcher, &QFileSystemWatcher::fileChanged,
            this, &ActiveJobsFolder::somethingChangedOnDisk);

    connect(&board, &JobPipelines::boardChanged,
            this, &ActiveJobsFolder::writeBoardChangesToFolders);
}

void ActiveJobsFolder::loadFromSettings()
{
    QSettings settings;
    chosenFolderPath = settings.value(folderPathSettingKey).toString();
}

QString ActiveJobsFolder::defaultFolderPath()
{
    QString documentsFolderPath =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documentsFolderPath.isEmpty()) {
        documentsFolderPath = QDir::homePath();
    }
    return QDir(documentsFolderPath).filePath(QStringLiteral("Job Crush/Active Jobs"));
}

QString ActiveJobsFolder::folderPath() const
{
    return chosenFolderPath.trimmed().isEmpty() ? defaultFolderPath() : chosenFolderPath;
}

void ActiveJobsFolder::startKeepingTheBoardAndFolderTheSame()
{
    hasStarted = true;
    if (!QDir().mkpath(folderPath())) {
        lastLookSentence = QStringLiteral(
            "Job Crush could not make the Active Jobs folder at %1. Pick a different "
            "folder in Settings.").arg(QDir::toNativeSeparators(folderPath()));
        emit folderStatusChanged();
        return;
    }
    lookAtEveryJobFolder();
}

bool ActiveJobsFolder::setFolderPath(const QString &newFolderPath, QString &reasonText)
{
    const QString cleanPath = QDir::cleanPath(newFolderPath.trimmed());
    if (cleanPath.isEmpty()) {
        reasonText = QStringLiteral("No folder was picked, so nothing changed.");
        return false;
    }
    if (!QDir().mkpath(cleanPath)) {
        reasonText = QStringLiteral("Job Crush could not use %1. Pick a folder you can "
                                    "save files in, such as one in Documents.")
                         .arg(QDir::toNativeSeparators(cleanPath));
        return false;
    }
    if (QDir(cleanPath) == QDir(folderPath())) {
        reasonText = QStringLiteral("That is already your Active Jobs folder.");
        return true;
    }

    // The old folder names mean nothing inside the new folder. The cards get
    // new job folders there, and jobs already in it are matched to the cards
    // they belong to.
    linkRepository.removeAllActiveJobFolders();
    links.clear();

    chosenFolderPath = cleanPath;
    QSettings settings;
    settings.setValue(folderPathSettingKey, chosenFolderPath);
    emit folderPathChanged();

    lookAtEveryJobFolder();
    reasonText = QStringLiteral("Your Active Jobs folder is now %1. The old folder was "
                                "left as it was.")
                     .arg(QDir::toNativeSeparators(chosenFolderPath));
    return true;
}

int ActiveJobsFolder::jobFolderCount() const
{
    return jobFoldersSeen;
}

QString ActiveJobsFolder::lastLookText() const
{
    return lastLookSentence;
}

QStringList ActiveJobsFolder::folderProblemSentences() const
{
    return problemSentences;
}

QString ActiveJobsFolder::jobFolderPathForCard(qint64 jobApplicationId) const
{
    const ActiveJobFolder *link = linkForCard(jobApplicationId);
    if (!link) {
        return QString();
    }
    const QString jobFolderPath = QDir(folderPath()).filePath(link->folderName);
    return QDir(jobFolderPath).exists() ? jobFolderPath : QString();
}

QString ActiveJobsFolder::instructionsForAnOutsideAi() const
{
    return AiBuddyInstructions::instructionsFor(folderPath());
}

// --- Reading the folder -----------------------------------------------------

void ActiveJobsFolder::somethingChangedOnDisk()
{
    // Each new change starts the wait again, so the read happens one second
    // after the LAST change, not the first.
    waitForWritingToStop.start();
}

void ActiveJobsFolder::lookAtEveryJobFolder()
{
    if (!hasStarted) {
        return;
    }

    followingTheFiles = true;
    problemSentences.clear();
    links = linkRepository.loadAllActiveJobFolders();

    int jobsAdded = 0;
    int jobsChanged = 0;
    jobFoldersSeen = 0;

    const QDir activeJobsFolder(folderPath());
    const QStringList folderNames =
        activeJobsFolder.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString &folderName : folderNames) {
        const QDir jobFolder(activeJobsFolder.filePath(folderName));
        const QString questFilePath = jobFolder.filePath(JobQuestFile::FileName);

        if (!jobFolder.exists(JobQuestFile::FileName)) {
            if (OlderJobFolderReader::isOlderJobFolder(jobFolder)) {
                reportProblem(folderName,
                              QStringLiteral("This folder has a HOW TO APPLY.txt but no "
                                             "JobQuest.txt. Use Import a folder in "
                                             "Settings to bring it in."));
            } else {
                reportProblem(folderName,
                              QStringLiteral("There is no JobQuest.txt in this folder, "
                                             "so Job Crush does not know what job it "
                                             "is."));
            }
            continue;
        }
        ++jobFoldersSeen;

        const QDateTime fileChangedTimestamp = lastChangedTimeOf(questFilePath);
        ActiveJobFolder *link = linkForFolder(folderName);
        if (link && link->fileChangedTimestamp.isValid()
                && link->fileChangedTimestamp == fileChangedTimestamp) {
            continue; // unchanged since Job Crush last read or wrote it
        }

        bool fileWasRead = false;
        const QString fileText = readWholeFile(questFilePath, fileWasRead);
        if (!fileWasRead) {
            reportProblem(folderName,
                          QStringLiteral("Job Crush could not open JobQuest.txt. If "
                                         "another program has it open, close it."));
            continue;
        }

        const JobQuestFile::ReadResult readResult = JobQuestFile::readQuestText(fileText);
        for (const QString &problemSentence : readResult.problemSentences) {
            reportProblem(folderName, problemSentence);
        }
        if (!readResult.hasWhatTheBoardNeeds) {
            continue; // read again once it is fixed, because its time will change
        }

        if (link) {
            followChangedFile(*link, readResult.quest, fileChangedTimestamp, folderName);
            ++jobsChanged;
        } else if (putNewFolderOnTheBoard(folderName, readResult.quest,
                                          fileChangedTimestamp)) {
            ++jobsAdded;
        }
    }

    followingTheFiles = false;

    // Cards that have no folder yet get one, and anything the user changed
    // on the board while the files were being read is written out.
    writeBoardChangesToFolders();
    watchEveryJobFolder();

    // The folders with a problem, counted once each however many problems
    // they have. Each sentence starts with "folder name: ", so the name is the
    // text before the first ": ". Windows does not allow a colon in a folder
    // name, so the name itself never holds one.
    QStringList foldersWithAProblem;
    for (const QString &problemSentence : std::as_const(problemSentences)) {
        const QString folderName = problemSentence.section(QStringLiteral(": "), 0, 0);
        if (!foldersWithAProblem.contains(folderName)) {
            foldersWithAProblem.append(folderName);
        }
    }

    QStringList sentenceParts;
    sentenceParts.append(jobFoldersSeen == 1
                             ? QStringLiteral("1 job folder.")
                             : QStringLiteral("%1 job folders.").arg(jobFoldersSeen));
    if (jobsAdded > 0) {
        sentenceParts.append(QStringLiteral("%1 new on your board.").arg(jobsAdded));
    }
    if (jobsChanged > 0) {
        sentenceParts.append(QStringLiteral("%1 updated from their files.").arg(jobsChanged));
    }
    if (!foldersWithAProblem.isEmpty()) {
        sentenceParts.append(foldersWithAProblem.count() == 1
                                 ? QStringLiteral("1 needs a fix, shown below.")
                                 : QStringLiteral("%1 need a fix, shown below.")
                                       .arg(foldersWithAProblem.count()));
    }
    lastLookSentence = sentenceParts.join(QLatin1Char(' '));
    emit folderStatusChanged();
}

void ActiveJobsFolder::followChangedFile(ActiveJobFolder &link, const JobQuest &quest,
                                         const QDateTime &fileChangedTimestamp,
                                         const QString &folderName)
{
    const qint64 jobApplicationId = link.jobApplicationId;

    TargetedJob card;
    bool cardFound = false;
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == jobApplicationId) {
            card = targetedJob;
            cardFound = true;
            break;
        }
    }

    // The user took this card off the board. The folder stays, and it is not
    // put back on the board: taking it off was a choice.
    if (!cardFound) {
        link.fileChangedTimestamp = fileChangedTimestamp;
        if (!linkRepository.updateActiveJobFolder(link)) {
            qWarning().noquote() << QStringLiteral(
                "ActiveJobsFolder::followChangedFile: could not save the folder link for "
                "\"%1\" — %2").arg(folderName, linkRepository.lastErrorText());
        }
        return;
    }

    // The column. Only a word Job Crush knows moves the card; the problem
    // list already says what is wrong with any other.
    PipelineStage fileStage = PipelineStage::Saved;
    if (JobQuestFile::readStatus(quest.statusText, fileStage)
            && pipelineStageToStorageText(fileStage) != link.syncedStageText) {
        board.moveToStage(jobApplicationId, fileStage);
        link.syncedStageText = pipelineStageToStorageText(fileStage);
    }

    // The applied date. A file with no Applied on line has not said anything
    // about the date, so the card keeps the one it has.
    QDate fileAppliedDate;
    if (JobQuestFile::readAppliedOn(quest.appliedOnText, QDate::currentDate(),
                                    fileAppliedDate)
            && fileAppliedDate != appliedDayOf(link.syncedAppliedTimestamp)) {
        QString reasonText;
        if (board.setAppliedDate(jobApplicationId, fileAppliedDate, reasonText)) {
            link.syncedAppliedTimestamp = QDateTime(fileAppliedDate, QTime(appliedHourOfDay, 0));
        } else {
            reportProblem(folderName, QStringLiteral("Applied on: %1").arg(reasonText));
        }
    }

    // The notes. Same rule: no Notes line means the file did not say, and
    // the user's notes on the card are kept. An outside AI that rewrites a
    // file and forgets the line must not wipe them out.
    if (!quest.notesText.isEmpty() && quest.notesText != link.syncedNotesText) {
        board.setNotesText(jobApplicationId, quest.notesText);
        link.syncedNotesText = quest.notesText;
    }

    // The facts about the job itself, when the file corrects them.
    JobPosting correctedPosting = card.posting;
    if (!quest.companyName.isEmpty()) {
        correctedPosting.companyName = quest.companyName;
    }
    if (!quest.positionTitle.isEmpty()) {
        correctedPosting.positionTitle = quest.positionTitle;
    }
    if (!quest.postingLink.isEmpty()) {
        correctedPosting.sourceUrl = quest.postingLink;
    }
    if (!quest.payText.isEmpty()) {
        correctedPosting.salaryText = quest.payText;
    }
    if (!quest.whereText.isEmpty()) {
        correctedPosting.locationText = quest.whereText;
        correctedPosting.isRemoteRole =
            quest.whereText.contains(QStringLiteral("remote"), Qt::CaseInsensitive);
    }
    if (correctedPosting.companyName != card.posting.companyName
            || correctedPosting.positionTitle != card.posting.positionTitle
            || correctedPosting.sourceUrl != card.posting.sourceUrl
            || correctedPosting.salaryText != card.posting.salaryText
            || correctedPosting.locationText != card.posting.locationText) {
        if (postingRepository.updateJobPostingDetails(correctedPosting)) {
            board.loadFromDatabase();
        } else {
            qWarning().noquote() << QStringLiteral(
                "ActiveJobsFolder::followChangedFile: could not save the corrected job "
                "from \"%1\" — %2").arg(folderName, postingRepository.lastErrorText());
        }
    }

    link.fileChangedTimestamp = fileChangedTimestamp;
    if (!linkRepository.updateActiveJobFolder(link)) {
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolder::followChangedFile: could not save the folder link for "
            "\"%1\" — %2").arg(folderName, linkRepository.lastErrorText());
    }
}

bool ActiveJobsFolder::putNewFolderOnTheBoard(const QString &folderName,
                                              const JobQuest &quest,
                                              const QDateTime &fileChangedTimestamp)
{
    JobPosting postingFromFile;
    postingFromFile.companyName = quest.companyName;
    postingFromFile.positionTitle = quest.positionTitle;
    postingFromFile.sourceUrl = quest.postingLink;
    postingFromFile.salaryText = quest.payText;
    postingFromFile.locationText = quest.whereText;
    postingFromFile.isRemoteRole =
        quest.whereText.contains(QStringLiteral("remote"), Qt::CaseInsensitive);
    postingFromFile.postingSource = quest.foundOn;
    postingFromFile.scoutSource = scoutSourceFor(quest.questCreatedBy);
    postingFromFile.discoveredTimestamp = QDateTime::currentDateTime();

    // Job Crush may already know this job: a sweep found it, or the user
    // added it by hand. Then the folder joins the job Job Crush already has,
    // rather than putting the same job up twice.
    const DuplicateJobDetector duplicateDetector;
    qint64 jobPostingId = 0;
    for (const JobPosting &knownPosting : postingRepository.loadAllJobPostings()) {
        if (duplicateDetector.sameJob(knownPosting, postingFromFile)) {
            jobPostingId = knownPosting.jobPostingId;
            break;
        }
    }

    const qint64 cardAlreadyOnBoard =
        jobPostingId != 0 ? board.jobApplicationIdForPosting(jobPostingId) : 0;
    if (cardAlreadyOnBoard != 0) {
        const ActiveJobFolder *otherLink = linkForCard(cardAlreadyOnBoard);
        if (otherLink) {
            reportProblem(folderName,
                          QStringLiteral("This is the same job as the folder \"%1\". Job "
                                         "Crush follows that one. Move anything you need "
                                         "into it, then delete this folder.")
                              .arg(otherLink->folderName));
            return false;
        }

        // Tie the folder to the card. The card was on the board first, so its
        // column, date and notes stand, and they are written into the file.
        // An import assumes every older job was sent; a card the user already
        // moved to Closed must not be dragged back to Applied by that guess.
        // The file's other lines, such as the pay, are kept.
        ActiveJobFolder newLink;
        newLink.folderName = folderName;
        newLink.jobApplicationId = cardAlreadyOnBoard;
        newLink.fileChangedTimestamp = fileChangedTimestamp;
        for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
            if (targetedJob.campaign.jobApplicationId == cardAlreadyOnBoard) {
                newLink.syncedAppliedTimestamp = targetedJob.campaign.appliedTimestamp;
                newLink.syncedNotesText = targetedJob.campaign.notesText;
            }
        }
        // Left empty so the next write puts the card's lines into the file.
        newLink.syncedStageText.clear();
        if (!linkRepository.insertActiveJobFolder(newLink)) {
            qWarning().noquote() << QStringLiteral(
                "ActiveJobsFolder::putNewFolderOnTheBoard: could not save the folder link "
                "for \"%1\" — %2").arg(folderName, linkRepository.lastErrorText());
            return false;
        }
        links.append(newLink);
        return true;
    }

    if (jobPostingId == 0) {
        if (!postingRepository.insertJobPosting(postingFromFile)) {
            reportProblem(folderName,
                          QStringLiteral("Job Crush could not save this job: %1.")
                              .arg(postingRepository.lastErrorText()));
            return false;
        }
        jobPostingId = postingFromFile.jobPostingId;
    }

    // The column the file names. With no Status line, a job with an applied
    // date was sent, and a job without one was not.
    PipelineStage stage = PipelineStage::Saved;
    QDate appliedDate;
    const bool fileHasAppliedDate = JobQuestFile::readAppliedOn(
        quest.appliedOnText, QDate::currentDate(), appliedDate);
    if (!JobQuestFile::readStatus(quest.statusText, stage)) {
        stage = fileHasAppliedDate ? PipelineStage::Applied : PipelineStage::Saved;
    }
    if (fileHasAppliedDate && appliedDate > QDate::currentDate()) {
        appliedDate = QDate(); // a future date is a typo; the problem list says so
    }
    // A job past Crushed was sent, so it has an applied date. When the file
    // gives none, the day the file was last changed is close enough, and the
    // user can fix it on the card.
    if (stage != PipelineStage::Saved && !appliedDate.isValid()) {
        appliedDate = fileChangedTimestamp.date();
    }
    const QDateTime appliedTimestamp = appliedDate.isValid()
        ? QDateTime(appliedDate, QTime(appliedHourOfDay, 0))
        : QDateTime();

    QString reasonText;
    const qint64 jobApplicationId = board.putOnBoardFromJobFolder(
        jobPostingId, stage, appliedTimestamp, quest.notesText, reasonText);
    if (jobApplicationId == 0) {
        reportProblem(folderName, reasonText);
        return false;
    }

    ActiveJobFolder newLink;
    newLink.folderName = folderName;
    newLink.jobApplicationId = jobApplicationId;
    newLink.fileChangedTimestamp = fileChangedTimestamp;
    newLink.syncedStageText = pipelineStageToStorageText(stage);
    newLink.syncedAppliedTimestamp = appliedTimestamp;
    newLink.syncedNotesText = quest.notesText;

    // The file did not give the column or the applied date the card now
    // has. Leaving them out of the synced values makes the next write put
    // them into the file, so the two agree.
    if (appliedTimestamp.isValid() && !fileHasAppliedDate) {
        newLink.syncedAppliedTimestamp = QDateTime();
    }
    PipelineStage stageInFile = PipelineStage::Saved;
    if (!JobQuestFile::readStatus(quest.statusText, stageInFile)) {
        newLink.syncedStageText.clear();
    }

    if (!linkRepository.insertActiveJobFolder(newLink)) {
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolder::putNewFolderOnTheBoard: could not save the folder link for "
            "\"%1\" — %2").arg(folderName, linkRepository.lastErrorText());
        return false;
    }
    links.append(newLink);
    return true;
}

// --- Writing the folder -----------------------------------------------------

void ActiveJobsFolder::writeBoardChangesToFolders()
{
    if (!hasStarted || followingTheFiles) {
        return;
    }
    if (!QDir(folderPath()).exists()) {
        return;
    }

    bool madeAnyFolder = false;
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        ActiveJobFolder *link = linkForCard(targetedJob.campaign.jobApplicationId);
        if (!link) {
            makeJobFolderForCard(targetedJob);
            madeAnyFolder = true;
            continue;
        }

        // A folder the user deleted is not made again.
        const QDir jobFolder(QDir(folderPath()).filePath(link->folderName));
        if (!jobFolder.exists()) {
            continue;
        }

        const QString boardStageText =
            pipelineStageToStorageText(targetedJob.campaign.pipelineStage);
        const bool stageChanged = boardStageText != link->syncedStageText;
        const bool appliedChanged = appliedDayOf(targetedJob.campaign.appliedTimestamp)
                                    != appliedDayOf(link->syncedAppliedTimestamp);
        const bool notesChanged = targetedJob.campaign.notesText != link->syncedNotesText;
        if (!stageChanged && !appliedChanged && !notesChanged) {
            continue;
        }

        const QString questFilePath = jobFolder.filePath(JobQuestFile::FileName);
        bool fileWasRead = false;
        const QString existingText = readWholeFile(questFilePath, fileWasRead);

        // Did the file change since Job Crush last looked? Then an outside AI
        // has written something Job Crush has not read yet. The board's change
        // is newer, so the three board lines are written, and every other line
        // the AI wrote is kept and read on the next look.
        const bool fileChangedSinceLastLook =
            fileWasRead && link->fileChangedTimestamp != lastChangedTimeOf(questFilePath);

        const QString newText = JobQuestFile::questTextWithBoardChanges(
            existingText,
            JobQuestFile::statusWordFor(targetedJob.campaign.pipelineStage),
            JobQuestFile::appliedOnTextFor(appliedDayOf(targetedJob.campaign.appliedTimestamp)),
            targetedJob.campaign.notesText);

        QDateTime fileChangedTimestamp;
        if (!writeQuestFile(link->folderName, newText, fileChangedTimestamp)) {
            continue;
        }
        link->syncedStageText = boardStageText;
        link->syncedAppliedTimestamp = targetedJob.campaign.appliedTimestamp;
        link->syncedNotesText = targetedJob.campaign.notesText;
        // An empty time never matches the file's time, so the next look reads
        // the file again and picks up the lines the outside AI wrote.
        link->fileChangedTimestamp =
            fileChangedSinceLastLook ? QDateTime() : fileChangedTimestamp;
        if (!linkRepository.updateActiveJobFolder(*link)) {
            qWarning().noquote() << QStringLiteral(
                "ActiveJobsFolder::writeBoardChangesToFolders: could not save the folder "
                "link for \"%1\" — %2").arg(link->folderName, linkRepository.lastErrorText());
        }
    }

    if (madeAnyFolder) {
        emit folderStatusChanged();
    }
}

void ActiveJobsFolder::makeJobFolderForCard(const TargetedJob &targetedJob)
{
    const QString folderName = uniqueFolderNameFor(
        targetedJob.posting.companyName + QStringLiteral(" - ")
        + targetedJob.posting.positionTitle);
    if (!QDir(folderPath()).mkpath(folderName)) {
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolder::makeJobFolderForCard: could not make the job folder \"%1\" "
            "— mkpath failed in \"%2\"").arg(folderName, folderPath());
        return;
    }

    JobQuest quest;
    quest.questCreatedBy = ScoutSourceText::displayNameFor(targetedJob.posting.scoutSource);
    quest.companyName = targetedJob.posting.companyName;
    quest.positionTitle = targetedJob.posting.positionTitle;
    quest.postingLink = targetedJob.posting.sourceUrl;
    quest.foundOn = targetedJob.posting.postingSource;
    quest.statusText = JobQuestFile::statusWordFor(targetedJob.campaign.pipelineStage);
    quest.appliedOnText = JobQuestFile::appliedOnTextFor(
        appliedDayOf(targetedJob.campaign.appliedTimestamp));
    quest.payText = targetedJob.posting.salaryText;
    quest.whereText = targetedJob.posting.locationText;
    quest.notesText = targetedJob.campaign.notesText;

    QDateTime fileChangedTimestamp;
    if (!writeQuestFile(folderName, JobQuestFile::questTextFor(quest), fileChangedTimestamp)) {
        return;
    }

    ActiveJobFolder newLink;
    newLink.folderName = folderName;
    newLink.jobApplicationId = targetedJob.campaign.jobApplicationId;
    newLink.fileChangedTimestamp = fileChangedTimestamp;
    newLink.syncedStageText = pipelineStageToStorageText(targetedJob.campaign.pipelineStage);
    newLink.syncedAppliedTimestamp = targetedJob.campaign.appliedTimestamp;
    newLink.syncedNotesText = targetedJob.campaign.notesText;
    if (!linkRepository.insertActiveJobFolder(newLink)) {
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolder::makeJobFolderForCard: could not save the folder link for "
            "\"%1\" — %2").arg(folderName, linkRepository.lastErrorText());
        return;
    }
    links.append(newLink);
    ++jobFoldersSeen;
}

bool ActiveJobsFolder::writeQuestFile(const QString &folderName, const QString &fileText,
                                      QDateTime &fileChangedTimestamp)
{
    const QString questFilePath =
        QDir(QDir(folderPath()).filePath(folderName)).filePath(JobQuestFile::FileName);

    // QSaveFile writes beside the file and swaps it in at the end, so an AI
    // reading at the same moment never sees half a file.
    QSaveFile questFile(questFilePath);
    if (!questFile.open(QIODevice::WriteOnly)
            || questFile.write(fileText.toUtf8()) < 0
            || !questFile.commit()) {
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolder::writeQuestFile: could not write \"%1\" — %2")
                .arg(questFilePath, questFile.errorString());
        return false;
    }
    fileChangedTimestamp = lastChangedTimeOf(questFilePath);
    return true;
}

void ActiveJobsFolder::watchEveryJobFolder()
{
    // The watch list is rebuilt from scratch each time. Folders come and go,
    // and a stale path on the list would never report anything.
    //
    // Three kinds of path are watched: the Active Jobs folder itself, so a new
    // job folder is seen; each job folder, so a new JobQuest.txt is seen; and
    // each JobQuest.txt, so an edit to it is seen.
    const QStringList watchedNow = diskWatcher.directories() + diskWatcher.files();
    if (!watchedNow.isEmpty()) {
        diskWatcher.removePaths(watchedNow);
    }

    QStringList pathsToWatch;
    const QDir activeJobsFolder(folderPath());
    pathsToWatch.append(activeJobsFolder.absolutePath());
    for (const QString &folderName :
         activeJobsFolder.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QDir jobFolder(activeJobsFolder.filePath(folderName));
        pathsToWatch.append(jobFolder.absolutePath());
        if (jobFolder.exists(JobQuestFile::FileName)) {
            pathsToWatch.append(jobFolder.filePath(JobQuestFile::FileName));
        }
    }
    diskWatcher.addPaths(pathsToWatch);
}

// --- Importing --------------------------------------------------------------

bool ActiveJobsFolder::importJobFolders(const QString &chosenFolderPath, QString &reasonText)
{
    const QDir chosenFolder(QDir::cleanPath(chosenFolderPath));
    if (!chosenFolder.exists()) {
        reasonText = QStringLiteral("That folder is not there any more. Pick it again.");
        return false;
    }
    const QString activeJobsPath = QDir(folderPath()).absolutePath();
    if (chosenFolder.absolutePath().startsWith(activeJobsPath, Qt::CaseInsensitive)) {
        reasonText = QStringLiteral("That folder is already inside your Active Jobs "
                                    "folder, so Job Crush is reading it already.");
        return false;
    }

    // The folder picked may be one job, or a folder of jobs. Only the folders
    // directly inside it are looked at. Deeper folders are left alone, so a
    // folder of jobs not yet sent, kept inside it, is never read.
    QList<QDir> jobFolders;
    if (isJobFolder(chosenFolder)) {
        jobFolders.append(chosenFolder);
    } else {
        for (const QString &folderName :
             chosenFolder.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            const QDir candidate(chosenFolder.filePath(folderName));
            if (isJobFolder(candidate)) {
                jobFolders.append(candidate);
            }
        }
    }
    if (jobFolders.isEmpty()) {
        reasonText = QStringLiteral(
            "No job folders in there. A job folder holds a JobQuest.txt or a HOW TO "
            "APPLY.txt. Pick the folder that holds the job folders.");
        return false;
    }

    if (!QDir().mkpath(activeJobsPath)) {
        // Each copy below then fails and is named in reasonText.
        qWarning().noquote() << QStringLiteral(
            "ActiveJobsFolder::importJobFolders: could not make the Active Jobs folder "
            "\"%1\" — mkpath failed").arg(activeJobsPath);
    }
    int jobsCopied = 0;
    int jobsAlreadyThere = 0;
    QStringList jobsThatFailed;

    for (const QDir &sourceFolder : jobFolders) {
        const QString folderName = folderNameWindowsAccepts(sourceFolder.dirName());
        if (QDir(activeJobsPath).exists(folderName)) {
            ++jobsAlreadyThere;
            continue;
        }

        // Read the older layout before copying. A copy is a new folder, and
        // its date would be today, not the day the application was sent.
        JobQuest olderQuest;
        const bool isOlder = OlderJobFolderReader::isOlderJobFolder(sourceFolder);
        if (isOlder) {
            QString problemSentence;
            if (!OlderJobFolderReader::readOlderJobFolder(sourceFolder, olderQuest,
                                                          problemSentence)) {
                jobsThatFailed.append(QStringLiteral("%1: %2")
                                          .arg(sourceFolder.dirName(), problemSentence));
                continue;
            }
        }

        const QString targetPath = QDir(activeJobsPath).filePath(folderName);
        if (!copyFolder(sourceFolder.absolutePath(), targetPath)) {
            jobsThatFailed.append(QStringLiteral("%1: Job Crush could not copy it.")
                                      .arg(sourceFolder.dirName()));
            continue;
        }
        if (isOlder) {
            // The time is not needed: the look below reads the new file anyway.
            // writeQuestFile logs a failed write itself.
            QDateTime ignoredTimestamp;
            writeQuestFile(folderName, JobQuestFile::questTextFor(olderQuest),
                           ignoredTimestamp);
        }
        ++jobsCopied;
    }

    lookAtEveryJobFolder();

    QStringList sentenceParts;
    sentenceParts.append(QStringLiteral("Brought in %1 jobs.").arg(jobsCopied));
    if (jobsAlreadyThere > 0) {
        sentenceParts.append(QStringLiteral("%1 were already in your Active Jobs folder, "
                                            "so they were left alone.")
                                 .arg(jobsAlreadyThere));
    }
    if (!jobsThatFailed.isEmpty()) {
        sentenceParts.append(QStringLiteral("%1 could not be brought in: %2")
                                 .arg(jobsThatFailed.count())
                                 .arg(jobsThatFailed.join(QStringLiteral(" "))));
    }
    sentenceParts.append(QStringLiteral("The original folders were not changed."));
    reasonText = sentenceParts.join(QLatin1Char(' '));
    return jobsCopied > 0;
}

// --- Small helpers ----------------------------------------------------------

QString ActiveJobsFolder::uniqueFolderNameFor(const QString &wantedName) const
{
    const QString cleanName = folderNameWindowsAccepts(wantedName);
    const QDir activeJobsFolder(folderPath());
    if (!activeJobsFolder.exists(cleanName)) {
        return cleanName;
    }
    // The name is taken. Try "Name (2)", "Name (3)" and so on until one is
    // free. The folder holds a finite number of names, so this always ends.
    for (int copyNumber = 2;; ++copyNumber) {
        const QString numberedName = QStringLiteral("%1 (%2)").arg(cleanName).arg(copyNumber);
        if (!activeJobsFolder.exists(numberedName)) {
            return numberedName;
        }
    }
}

ActiveJobFolder *ActiveJobsFolder::linkForFolder(const QString &folderName)
{
    for (ActiveJobFolder &link : links) {
        if (link.folderName.compare(folderName, Qt::CaseInsensitive) == 0) {
            return &link;
        }
    }
    return nullptr;
}

ActiveJobFolder *ActiveJobsFolder::linkForCard(qint64 jobApplicationId)
{
    for (ActiveJobFolder &link : links) {
        if (link.jobApplicationId == jobApplicationId) {
            return &link;
        }
    }
    return nullptr;
}

const ActiveJobFolder *ActiveJobsFolder::linkForCard(qint64 jobApplicationId) const
{
    for (const ActiveJobFolder &link : links) {
        if (link.jobApplicationId == jobApplicationId) {
            return &link;
        }
    }
    return nullptr;
}

void ActiveJobsFolder::reportProblem(const QString &folderName, const QString &problemSentence)
{
    problemSentences.append(QStringLiteral("%1: %2").arg(folderName, problemSentence));
}
