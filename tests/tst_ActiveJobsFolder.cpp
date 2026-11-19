#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "IsolatedTestSettings.h"
#include "model/ActiveJobFolderRepository.h"
#include "model/JobApplicationRepository.h"
#include "model/JobCrushDatabase.h"
#include "model/JobPostingRepository.h"
#include "model/PostingSourceRepository.h"
#include "logic/activejobs/ActiveJobsFolder.h"
#include "logic/activejobs/JobQuestFile.h"
#include "logic/aiapps/AiAppRequests.h"
#include "logic/aiapps/AiAppTools.h"
#include "logic/pipelines/JobPipelines.h"

namespace {

// Writes UTF-8 text to a file, making its folders first. Fails the test if
// the file cannot be opened.
void writeTextFile(const QString &filePath, const QString &fileText)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(fileText.toUtf8());
}

// Reads a file as UTF-8 text. A missing file reads as empty text, so a test
// can check that a file was not written.
QString readTextFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

// Moves a file's last-changed time on, so a test never depends on two writes
// landing in different milliseconds.
void markChangedLater(const QString &filePath, int secondsLater)
{
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(secondsLater),
                             QFileDevice::FileModificationTime));
}

} // namespace

// Tests for ActiveJobsFolder: keeping the board and the Active Jobs folder
// the same, in both directions. Runs on a temporary folder with its own
// database file in it, so nothing on the real computer is touched.
class ActiveJobsFolderTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir settingsFolder;
    QTemporaryDir workFolder;
    QString activeJobsPath;

    std::unique_ptr<JobCrushDatabase> database;
    std::unique_ptr<JobPostingRepository> postingRepository;
    std::unique_ptr<PostingSourceRepository> postingSourceRepository;
    std::unique_ptr<JobApplicationRepository> applicationRepository;
    std::unique_ptr<ActiveJobFolderRepository> linkRepository;
    std::unique_ptr<JobPipelines> board;
    std::unique_ptr<ActiveJobsFolder> activeJobs;

    // The path of the JobQuest.txt file inside one job folder.
    QString questPath(const QString &folderName) const
    {
        return QDir(QDir(activeJobsPath).filePath(folderName)).filePath(QStringLiteral("JobQuest.txt"));
    }

    // The one card on the board. Most tests put exactly one job on it. Any
    // other count is logged and returns an empty card, so the checks that
    // follow fail with a clear reason.
    TargetedJob onlyCard() const
    {
        const QList<TargetedJob> cards = board->everyTargetedJob();
        if (cards.count() != 1) {
            qWarning().noquote() << QStringLiteral(
                "ActiveJobsFolderTest::onlyCard: expected one card on the board — found %1")
                    .arg(cards.count());
            return TargetedJob{};
        }
        return cards.first();
    }

private slots:
    void initTestCase()
    {
        useIsolatedTestSettings(settingsFolder);
    }

    void init()
    {
        // A fresh folder and a fresh database for every test.
        activeJobsPath = QDir(workFolder.path()).filePath(
            QStringLiteral("Active Jobs %1").arg(QDateTime::currentMSecsSinceEpoch()));
        QSettings().setValue(QStringLiteral("activeJobs/folderPath"), activeJobsPath);

        // Tear down in reverse order of building, then let go of the
        // connection name so the next database can use it.
        activeJobs.reset();
        board.reset();
        linkRepository.reset();
        applicationRepository.reset();
        postingSourceRepository.reset();
        postingRepository.reset();
        database.reset();
        QSqlDatabase::removeDatabase(QStringLiteral("jobCrushPrimaryConnection"));

        database = std::make_unique<JobCrushDatabase>();
        // A database file in the temporary folder. ":memory:" cannot be
        // used: Qt reads a leading colon as one of its own resource paths.
        QVERIFY2(database->openAtFilePath(QDir(workFolder.path()).filePath(
                     QStringLiteral("jobcrush-%1.sqlite").arg(QDateTime::currentMSecsSinceEpoch()))),
                 qPrintable(database->lastErrorText()));
        postingRepository = std::make_unique<JobPostingRepository>(*database);
        postingSourceRepository = std::make_unique<PostingSourceRepository>(*database);
        applicationRepository = std::make_unique<JobApplicationRepository>(*database);
        linkRepository = std::make_unique<ActiveJobFolderRepository>(*database);
        board = std::make_unique<JobPipelines>(*applicationRepository, *postingRepository,
                                               *postingSourceRepository);
        board->loadFromDatabase();
        activeJobs = std::make_unique<ActiveJobsFolder>(*board, *postingRepository,
                                                        *linkRepository);
        activeJobs->loadFromSettings();
    }

    void aNewFolderPutsAJobOnTheBoard()
    {
        writeTextFile(questPath(QStringLiteral("Acme - Qt Dev")), QStringLiteral(
            "Quest created by: ChatGPT\nCompany: Acme\nTitle: Qt Dev\n"
            "Found on: FlexJobs\nStatus: Applied\nApplied on: 10/1/2026\n"
            "Where: Remote\nNotes: Jane is the recruiter\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();

        const TargetedJob card = onlyCard();
        QCOMPARE(card.posting.companyName, QStringLiteral("Acme"));
        QCOMPARE(card.posting.scoutSource, QStringLiteral("ChatGPT"));
        QCOMPARE(card.posting.postingSource, QStringLiteral("FlexJobs"));
        QVERIFY(card.posting.isRemoteRole);
        QCOMPARE(card.campaign.pipelineStage, PipelineStage::Applied);
        QCOMPARE(card.campaign.appliedTimestamp.date(), QDate(2026, 10, 1));
        QCOMPARE(card.campaign.notesText, QStringLiteral("Jane is the recruiter"));
        QVERIFY(activeJobs->folderProblemSentences().isEmpty());
        QCOMPARE(activeJobs->jobFolderCount(), 1);
        QVERIFY(!activeJobs->jobFolderPathForCard(card.campaign.jobApplicationId).isEmpty());
    }

    void aChangedFileMovesTheCard()
    {
        const QString filePath = questPath(QStringLiteral("Acme - Qt Dev"));
        writeTextFile(filePath, QStringLiteral(
            "Company: Acme\nTitle: Qt Dev\nStatus: Applied\nApplied on: 10/1/2026\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Applied);

        writeTextFile(filePath, QStringLiteral(
            "Company: Acme\nTitle: Qt Developer\nStatus: Interview\n"
            "Applied on: 10/1/2026\nNotes: Call on Monday\n"));
        markChangedLater(filePath, 5);
        activeJobs->lookAtEveryJobFolder();

        const TargetedJob card = onlyCard();
        QCOMPARE(card.campaign.pipelineStage, PipelineStage::Interview);
        QCOMPARE(card.campaign.notesText, QStringLiteral("Call on Monday"));
        QCOMPARE(card.posting.positionTitle, QStringLiteral("Qt Developer"));
    }

    void movingACardChangesItsFile()
    {
        const QString filePath = questPath(QStringLiteral("Acme - Qt Dev"));
        writeTextFile(filePath, QStringLiteral(
            "Company: Acme\nTitle: Qt Dev\nRecruiter: Jane\nStatus: Applied\n"
            "Applied on: 10/1/2026\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();

        const qint64 cardId = onlyCard().campaign.jobApplicationId;
        QVERIFY(board->moveToStage(cardId, PipelineStage::Closed));
        QVERIFY(board->setNotesText(cardId, QStringLiteral("They said no")));

        const QString fileText = readTextFile(filePath);
        QVERIFY2(fileText.contains(QStringLiteral("Status: Closed")), qPrintable(fileText));
        QVERIFY(fileText.contains(QStringLiteral("Notes: They said no")));
        QVERIFY(fileText.contains(QStringLiteral("Recruiter: Jane")));
        QVERIFY(fileText.contains(QStringLiteral("Applied on: 10/01/2026")));

        // Job Crush reading its own write must not change anything.
        activeJobs->lookAtEveryJobFolder();
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Closed);
    }

    void anAiChangingTheStatusToAppliedStampsTheDateAndTime()
    {
        const QString filePath = questPath(QStringLiteral("Acme - Qt Dev"));
        writeTextFile(filePath, QStringLiteral("Company: Acme\nTitle: Qt Dev\nStatus: Crushed\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();
        QVERIFY(!onlyCard().campaign.appliedTimestamp.isValid());

        // The outside AI moves the job to Applied and gives no date.
        writeTextFile(filePath, QStringLiteral("Company: Acme\nTitle: Qt Dev\nStatus: Applied\n"));
        markChangedLater(filePath, 5);
        const QDateTime beforeTheLook = QDateTime::currentDateTime().addSecs(-1);
        activeJobs->lookAtEveryJobFolder();

        const QDateTime stamped = onlyCard().campaign.appliedTimestamp;
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Applied);
        QVERIFY(stamped.isValid());
        QVERIFY(stamped >= beforeTheLook);

        // The file gets the date and the time written back into it.
        const QString fileText = readTextFile(filePath);
        QVERIFY2(fileText.contains(QStringLiteral("Applied on: %1")
                                       .arg(JobQuestFile::appliedOnTextFor(stamped))),
                 qPrintable(fileText));
    }

    void anAiWritingTodaysDateKeepsTheStampedTime()
    {
        const QString filePath = questPath(QStringLiteral("Acme - Qt Dev"));
        writeTextFile(filePath, QStringLiteral("Company: Acme\nTitle: Qt Dev\nStatus: Crushed\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();

        // The AI writes Applied and today's date, with no time.
        writeTextFile(filePath, QStringLiteral(
            "Company: Acme\nTitle: Qt Dev\nStatus: Applied\nApplied on: %1\n")
                .arg(JobQuestFile::appliedOnTextFor(QDate::currentDate())));
        markChangedLater(filePath, 5);
        activeJobs->lookAtEveryJobFolder();

        // The move stamped the time. The file's date is the same day, so it
        // must not replace that time with noon.
        const QDateTime stamped = onlyCard().campaign.appliedTimestamp;
        QCOMPARE(stamped.date(), QDate::currentDate());
        QVERIFY(qAbs(stamped.secsTo(QDateTime::currentDateTime())) < 60);
    }

    void statusEscapedMovesTheCardWithNoAppliedDate()
    {
        const QString filePath = questPath(QStringLiteral("Acme - Qt Dev"));
        writeTextFile(filePath, QStringLiteral("Company: Acme\nTitle: Qt Dev\nStatus: Crushed\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();

        // The outside AI changes one line: Status: Escaped.
        writeTextFile(filePath, QStringLiteral("Company: Acme\nTitle: Qt Dev\nStatus: Escaped\n"));
        markChangedLater(filePath, 5);
        activeJobs->lookAtEveryJobFolder();

        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Escaped);
        QVERIFY(!onlyCard().campaign.appliedTimestamp.isValid());
        // Escaped jobs are not open, so Staging and the still-listed check skip them.
        QVERIFY(board->everyOpenTargetedJob().isEmpty());
    }

    void closeFromDiscoveriesKeepsARecordOnTheBoard()
    {
        JobPosting posting;
        posting.companyName = QStringLiteral("Golf");
        posting.positionTitle = QStringLiteral("Qt Dev");
        posting.discoveredTimestamp = QDateTime::currentDateTime();
        QVERIFY(postingRepository->insertJobPosting(posting));

        // Not on the board yet: a card is made straight in Closed, with no date.
        QString reasonText;
        QVERIFY2(board->closeJobPosting(posting.jobPostingId, reasonText),
                 qPrintable(reasonText));
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Closed);
        QVERIFY(!onlyCard().campaign.appliedTimestamp.isValid());

        // Closing it again changes nothing and says so.
        QVERIFY(!board->closeJobPosting(posting.jobPostingId, reasonText));
        QCOMPARE(board->everyTargetedJob().count(), 1);

        // A card already on the board moves to Closed and keeps its date.
        const qint64 cardId = onlyCard().campaign.jobApplicationId;
        QVERIFY(board->moveToStage(cardId, PipelineStage::Applied));
        const QDateTime appliedTimestamp = onlyCard().campaign.appliedTimestamp;
        QVERIFY(board->closeJobPosting(posting.jobPostingId, reasonText));
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Closed);
        QCOMPARE(onlyCard().campaign.appliedTimestamp, appliedTimestamp);
    }

    void aCardCrushedInJobCrushGetsAFolder()
    {
        activeJobs->startKeepingTheBoardAndFolderTheSame();

        JobPosting posting;
        posting.companyName = QStringLiteral("Beta Corp");
        posting.positionTitle = QStringLiteral("QML Engineer");
        posting.postingSource = QStringLiteral("greenhouse");
        posting.scoutSource = QStringLiteral("scout");
        posting.discoveredTimestamp = QDateTime::currentDateTime();
        QVERIFY(postingRepository->insertJobPosting(posting));
        QString reasonText;
        QVERIFY(board->crushJobPosting(posting.jobPostingId, reasonText));

        const QString fileText = readTextFile(questPath(QStringLiteral("Beta Corp - QML Engineer")));
        QVERIFY2(fileText.contains(QStringLiteral("Quest created by: Job Crush")),
                 qPrintable(fileText));
        QVERIFY(fileText.contains(QStringLiteral("Status: Crushed")));
    }

    void aBrokenFileIsReportedAndNotPutOnTheBoard()
    {
        writeTextFile(questPath(QStringLiteral("Mystery")), QStringLiteral("Company: Acme\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();
        QCOMPARE(board->everyTargetedJob().count(), 0);
        QCOMPARE(activeJobs->folderProblemSentences().count(), 1);
        QVERIFY(activeJobs->folderProblemSentences().first().startsWith(QStringLiteral("Mystery: ")));
    }

    void aDeletedFolderIsNotMadeAgain()
    {
        const QString filePath = questPath(QStringLiteral("Acme - Qt Dev"));
        writeTextFile(filePath, QStringLiteral("Company: Acme\nTitle: Qt Dev\n"));
        activeJobs->startKeepingTheBoardAndFolderTheSame();
        const qint64 cardId = onlyCard().campaign.jobApplicationId;

        QVERIFY(QDir(QFileInfo(filePath).absolutePath()).removeRecursively());
        QVERIFY(board->moveToStage(cardId, PipelineStage::Applied));
        QVERIFY(!QFile::exists(filePath));
        QCOMPARE(QDir(activeJobsPath).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count(), 0);
    }

    void importReadsTheOlderLayoutAndLeavesTheOriginalAlone()
    {
        activeJobs->startKeepingTheBoardAndFolderTheSame();

        const QString importPath = QDir(workFolder.path()).filePath(
            QStringLiteral("Application Complete %1").arg(QDateTime::currentMSecsSinceEpoch()));
        const QString howToApplyPath = QDir(importPath).filePath(
            QStringLiteral("Shield AI - Staff Engineer GCS/HOW TO APPLY.txt"));
        writeTextFile(howToApplyPath, QStringLiteral(
            "HOW: Apply at https://jobs.lever.co/shieldai/abc-123.\n"
            "PAY: $180k - $220k\nWHERE: San Diego, CA\n"));
        writeTextFile(QDir(importPath).filePath(
                          QStringLiteral("Shield AI - Staff Engineer GCS/resume.docx")),
                      QStringLiteral("not really a docx"));
        // A folder of jobs not yet sent, kept inside. It must not be read.
        writeTextFile(QDir(importPath).filePath(
                          QStringLiteral("1 - Send These/Other - Job/HOW TO APPLY.txt")),
                      QStringLiteral("HOW: https://example.com\n"));

        QString reasonText;
        QVERIFY2(activeJobs->importJobFolders(importPath, reasonText), qPrintable(reasonText));

        const TargetedJob card = onlyCard();
        QCOMPARE(card.posting.companyName, QStringLiteral("Shield AI"));
        QCOMPARE(card.posting.positionTitle, QStringLiteral("Staff Engineer GCS"));
        QCOMPARE(card.posting.sourceUrl, QStringLiteral("https://jobs.lever.co/shieldai/abc-123"));
        QCOMPARE(card.posting.salaryText, QStringLiteral("$180k - $220k"));
        QCOMPARE(card.posting.locationText, QStringLiteral("San Diego, CA"));
        QCOMPARE(card.campaign.pipelineStage, PipelineStage::Applied);
        QVERIFY(card.campaign.appliedTimestamp.isValid());

        const QString copiedFolder = QDir(activeJobsPath).filePath(
            QStringLiteral("Shield AI - Staff Engineer GCS"));
        QVERIFY(QFile::exists(QDir(copiedFolder).filePath(QStringLiteral("JobQuest.txt"))));
        QVERIFY(QFile::exists(QDir(copiedFolder).filePath(QStringLiteral("resume.docx"))));
        QVERIFY(!QFile::exists(QFileInfo(howToApplyPath).absoluteDir()
                                   .filePath(QStringLiteral("JobQuest.txt"))));

        // Importing again changes nothing.
        QVERIFY(!activeJobs->importJobFolders(importPath, reasonText));
        QCOMPARE(board->everyTargetedJob().count(), 1);
    }

    void anImportedJobAlreadyOnTheBoardKeepsItsColumn()
    {
        activeJobs->startKeepingTheBoardAndFolderTheSame();

        JobPosting posting;
        posting.companyName = QStringLiteral("Shield AI");
        posting.positionTitle = QStringLiteral("Staff Engineer GCS");
        posting.sourceUrl = QStringLiteral("https://jobs.lever.co/shieldai/abc-123");
        posting.scoutSource = QStringLiteral("you");
        posting.discoveredTimestamp = QDateTime::currentDateTime();
        QVERIFY(postingRepository->insertJobPosting(posting));
        QString reasonText;
        QVERIFY(board->crushJobPosting(posting.jobPostingId, reasonText));
        const qint64 cardId = onlyCard().campaign.jobApplicationId;
        QVERIFY(board->moveToStage(cardId, PipelineStage::Closed));

        // The card already has a folder. Take it away, the way a user who
        // never opened it might delete it, then import the older folder.
        QVERIFY(QDir(QDir(activeJobsPath).filePath(
                         QStringLiteral("Shield AI - Staff Engineer GCS"))).removeRecursively());
        linkRepository->removeAllActiveJobFolders();

        const QString importPath = QDir(workFolder.path()).filePath(
            QStringLiteral("Older %1").arg(QDateTime::currentMSecsSinceEpoch()));
        writeTextFile(QDir(importPath).filePath(
                          QStringLiteral("Shield AI - Staff Engineer GCS/HOW TO APPLY.txt")),
                      QStringLiteral("HOW: https://jobs.lever.co/shieldai/abc-123\n"));
        QVERIFY2(activeJobs->importJobFolders(importPath, reasonText), qPrintable(reasonText));

        QCOMPARE(board->everyTargetedJob().count(), 1);
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Closed);
        const QString fileText = readTextFile(questPath(QStringLiteral("Shield AI - Staff Engineer GCS")));
        QVERIFY2(fileText.contains(QStringLiteral("Status: Closed")), qPrintable(fileText));
    }

    // --- AI apps working the board (AiAppRequests) ------------------------

    void anAiAppAddsAJobAndItGetsAFolder()
    {
        activeJobs->startKeepingTheBoardAndFolderTheSame();
        const AiAppRequests requests(*board, *postingRepository, *activeJobs);
        const QDate today(2026, 10, 16);

        const AiAppAnswer added = requests.runAction(
            AiAppTools::AddJob,
            QJsonObject { { QStringLiteral("company"), QStringLiteral("Gamma Labs") },
                          { QStringLiteral("title"), QStringLiteral("C++ Engineer") },
                          { QStringLiteral("applied_on"), QStringLiteral("yesterday") },
                          { QStringLiteral("created_by"), QStringLiteral("Claude") } },
            today);
        QVERIFY2(!added.isError, qPrintable(added.text));

        const TargetedJob card = onlyCard();
        QCOMPARE(card.campaign.pipelineStage, PipelineStage::Applied);
        QCOMPARE(card.campaign.appliedTimestamp.date(), QDate(2026, 10, 15));
        QCOMPARE(card.posting.scoutSource, QStringLiteral("Claude"));
        const QString fileText = readTextFile(questPath(QStringLiteral("Gamma Labs - C++ Engineer")));
        QVERIFY2(fileText.contains(QStringLiteral("Quest created by: Claude")), qPrintable(fileText));

        // The same job again is not added twice.
        const AiAppAnswer again = requests.runAction(
            AiAppTools::AddJob,
            QJsonObject { { QStringLiteral("company"), QStringLiteral("Gamma Labs, Inc.") },
                          { QStringLiteral("title"), QStringLiteral("C++ Engineer") } },
            today);
        QVERIFY(!again.isError);
        QVERIFY2(again.text.contains(QStringLiteral("already on the board")), qPrintable(again.text));
        QCOMPARE(board->everyTargetedJob().count(), 1);
    }

    void anAiAppMovingAJobPastCrushedStampsTheDate()
    {
        activeJobs->startKeepingTheBoardAndFolderTheSame();
        const AiAppRequests requests(*board, *postingRepository, *activeJobs);
        const QDate today = QDate::currentDate();

        QVERIFY(!requests.runAction(
                    AiAppTools::AddJob,
                    QJsonObject { { QStringLiteral("company"), QStringLiteral("Echo") },
                                  { QStringLiteral("title"), QStringLiteral("C++ Dev") } },
                    today).isError);
        const qint64 cardId = onlyCard().campaign.jobApplicationId;
        QVERIFY(!onlyCard().campaign.appliedTimestamp.isValid());

        // Straight from Crushed to Interview, with no applied date given.
        QVERIFY(!requests.runAction(
                    AiAppTools::MoveJob,
                    QJsonObject { { QStringLiteral("job_id"), cardId },
                                  { QStringLiteral("column"), QStringLiteral("Interview") } },
                    today).isError);
        QVERIFY(onlyCard().campaign.appliedTimestamp.isValid());
        QVERIFY(qAbs(onlyCard().campaign.appliedTimestamp.secsTo(
                    QDateTime::currentDateTime())) < 60);

        // add_job into Applied with no date is stamped right now too.
        QVERIFY(!requests.runAction(
                    AiAppTools::AddJob,
                    QJsonObject { { QStringLiteral("company"), QStringLiteral("Foxtrot") },
                                  { QStringLiteral("title"), QStringLiteral("Qt Lead") },
                                  { QStringLiteral("column"), QStringLiteral("Applied") } },
                    today).isError);
        for (const TargetedJob &card : board->everyTargetedJob()) {
            if (card.posting.companyName == QStringLiteral("Foxtrot")) {
                QVERIFY(qAbs(card.campaign.appliedTimestamp.secsTo(
                            QDateTime::currentDateTime())) < 60);
            }
        }
    }

    void anAiAppMovesMarksAndNotesAJob()
    {
        activeJobs->startKeepingTheBoardAndFolderTheSame();
        const AiAppRequests requests(*board, *postingRepository, *activeJobs);
        const QDate today(2026, 10, 16);

        QVERIFY(!requests.runAction(
                    AiAppTools::AddJob,
                    QJsonObject { { QStringLiteral("company"), QStringLiteral("Delta") },
                                  { QStringLiteral("title"), QStringLiteral("QML Dev") } },
                    today).isError);
        const qint64 cardId = onlyCard().campaign.jobApplicationId;
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Saved);

        const QJsonObject jobId { { QStringLiteral("job_id"), cardId } };
        QJsonObject markArguments = jobId;
        markArguments.insert(QStringLiteral("applied_on"), QStringLiteral("10/2/2026"));
        QVERIFY(!requests.runAction(AiAppTools::MarkApplied, markArguments, today).isError);
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Applied);
        QCOMPARE(onlyCard().campaign.appliedTimestamp.date(), QDate(2026, 10, 2));

        QJsonObject noteArguments = jobId;
        noteArguments.insert(QStringLiteral("note"), QStringLiteral("Sent the portfolio"));
        QVERIFY(!requests.runAction(AiAppTools::AddNote, noteArguments, today).isError);
        QCOMPARE(onlyCard().campaign.notesText, QStringLiteral("10/16/2026: Sent the portfolio"));

        // Applied more than two weeks ago: whats_waiting says to follow up.
        const AiAppAnswer waiting =
            requests.runAction(AiAppTools::WhatsWaiting, QJsonObject {}, QDate(2026, 10, 20));
        QVERIFY2(waiting.text.contains(QStringLiteral("follow up")), qPrintable(waiting.text));

        // mark_applied with no date keeps the date the card already has.
        QVERIFY(!requests.runAction(AiAppTools::MarkApplied, jobId, today).isError);
        QCOMPARE(onlyCard().campaign.appliedTimestamp.date(), QDate(2026, 10, 2));

        QJsonObject moveArguments = jobId;
        moveArguments.insert(QStringLiteral("column"), QStringLiteral("Rejected"));
        QVERIFY(!requests.runAction(AiAppTools::MoveJob, moveArguments, today).isError);
        QCOMPARE(onlyCard().campaign.pipelineStage, PipelineStage::Closed);

        const AiAppAnswer missing = requests.runAction(
            AiAppTools::MoveJob,
            QJsonObject { { QStringLiteral("job_id"), 9999 },
                          { QStringLiteral("column"), QStringLiteral("Offer") } },
            today);
        QVERIFY(missing.isError);
    }
};

QTEST_GUILESS_MAIN(ActiveJobsFolderTest)
#include "tst_ActiveJobsFolder.moc"
