#include <QtTest>

#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "logic/jobscout/DiscoveryWorker.h"
#include "model/JobCrushDatabase.h"
#include "model/JobPostingRepository.h"

namespace {

// More jobs than DiscoveryWorker saves in one batch (200), so the save has
// to run over more than one batch.
constexpr int jobsInMoreThanOneBatch = 450;

// A job as a sweep would hand it over: Acme, on one board, with its own id.
JobPosting foundJob(const QString &externalSourceId, const QString &positionTitle,
                    const QString &locationText = QStringLiteral("Pittsburgh, PA"))
{
    JobPosting jobPosting;
    jobPosting.companyName = QStringLiteral("Acme");
    jobPosting.positionTitle = positionTitle;
    jobPosting.locationText = locationText;
    jobPosting.postingSource = QStringLiteral("employerboards");
    jobPosting.externalSourceId = externalSourceId;
    jobPosting.sourceUrl = QStringLiteral("https://boards.greenhouse.io/acme/jobs/%1")
                               .arg(externalSourceId);
    jobPosting.discoveredTimestamp = QDateTime::currentDateTime();
    jobPosting.postedTimestamp = QDateTime::currentDateTime();
    return jobPosting;
}

} // namespace

// Tests for DiscoveryWorker: saving found jobs, and preparing the rows for a
// Discoveries tab. The worker is called straight from the test here, on the
// test's own thread. In the app it runs on a background thread, but what it
// does is the same. Uses a database in a temporary folder.
class DiscoveryWorkerTest : public QObject {
    Q_OBJECT

private:
    QTemporaryDir workFolder;
    QString databaseFilePath;
    std::unique_ptr<JobCrushDatabase> mainDatabase;
    std::unique_ptr<DiscoveryWorker> discoveryWorker;

private slots:
    void init()
    {
        QVERIFY(workFolder.isValid());
        static int databaseNumber = 0;
        databaseFilePath = workFolder.filePath(
            QStringLiteral("jobcrush-%1.sqlite").arg(++databaseNumber));

        // The main connection makes the schema first, the way the app does.
        mainDatabase = std::make_unique<JobCrushDatabase>();
        QVERIFY(mainDatabase->openAtFilePath(databaseFilePath));

        discoveryWorker = std::make_unique<DiscoveryWorker>(databaseFilePath);
        discoveryWorker->openDatabase();
    }

    void cleanup()
    {
        discoveryWorker.reset();
        mainDatabase.reset();
    }

    void savesNewJobsAndCountsKnownOnes()
    {
        QSignalSpy savedSpy(discoveryWorker.get(), &DiscoveryWorker::findingsSaved);

        // findingsSaved's second argument is the save outcome.
        discoveryWorker->saveFindings(QStringLiteral("employerboards"),
                                      { foundJob(QStringLiteral("1"), QStringLiteral("Qt Developer")),
                                        foundJob(QStringLiteral("2"), QStringLiteral("C++ Engineer")) });
        QCOMPARE(savedSpy.count(), 1);
        FindingsSaveOutcome firstOutcome = savedSpy.at(0).at(1).value<FindingsSaveOutcome>();
        QCOMPARE(firstOutcome.jobsSaved, 2);
        QCOMPARE(firstOutcome.newJobs, 2);
        QCOMPARE(firstOutcome.jobsRefused, 0);

        // The same two again, plus one new one: only the new one is saved.
        discoveryWorker->saveFindings(QStringLiteral("employerboards"),
                                      { foundJob(QStringLiteral("1"), QStringLiteral("Qt Developer")),
                                        foundJob(QStringLiteral("2"), QStringLiteral("C++ Engineer")),
                                        foundJob(QStringLiteral("3"), QStringLiteral("QML Developer")) });
        QCOMPARE(savedSpy.count(), 2);
        const FindingsSaveOutcome secondOutcome = savedSpy.at(1).at(1).value<FindingsSaveOutcome>();
        QCOMPARE(secondOutcome.jobsSaved, 3);
        QCOMPARE(secondOutcome.newJobs, 1);

        JobPostingRepository mainRepository(*mainDatabase);
        QCOMPARE(mainRepository.loadAllJobPostings().count(), 3);
    }

    void savesMoreJobsThanOneBatchHolds()
    {
        QList<JobPosting> manyJobs;
        for (int jobNumber = 0; jobNumber < jobsInMoreThanOneBatch; ++jobNumber) {
            manyJobs.append(foundJob(QString::number(jobNumber),
                                     QStringLiteral("Qt Developer %1").arg(jobNumber)));
        }
        QSignalSpy savedSpy(discoveryWorker.get(), &DiscoveryWorker::findingsSaved);
        discoveryWorker->saveFindings(QStringLiteral("employerboards"), manyJobs);

        const FindingsSaveOutcome saveOutcome = savedSpy.at(0).at(1).value<FindingsSaveOutcome>();
        QCOMPARE(saveOutcome.newJobs, jobsInMoreThanOneBatch);
        JobPostingRepository mainRepository(*mainDatabase);
        QCOMPARE(mainRepository.loadAllJobPostings().count(), jobsInMoreThanOneBatch);
    }

    // The user pressed "still open" while a check was waiting on the
    // network. The check's "gone" must not land.
    void aJobTheUserSaysIsOpenIsNeverMarkedGone()
    {
        discoveryWorker->saveFindings(QStringLiteral("employerboards"),
                                      { foundJob(QStringLiteral("1"), QStringLiteral("Qt Developer")) });
        JobPostingRepository mainRepository(*mainDatabase);
        const qint64 jobPostingId = mainRepository.loadAllJobPostings().first().jobPostingId;

        QVERIFY(mainRepository.markStillOpenByUser(jobPostingId));
        QVERIFY(mainRepository.updatePostingGoneSince(jobPostingId, QDateTime::currentDateTime()));
        bool jobPostingWasFound = false;
        QVERIFY(!mainRepository.loadJobPostingById(jobPostingId, jobPostingWasFound)
                     .postingGoneSinceTimestamp.isValid());
        QVERIFY(jobPostingWasFound);
    }

    // Qt Developer ranks above Nurse. The Berlin job is outside the search
    // area, so it is held back from Top Prospects, counted, and listed in
    // its own set of rows.
    // The request numbers 7 and 8 are any numbers: the worker sends each one
    // back with its rows, so the caller can match answer to request.
    void topProspectsAreRankedAndTheHeldBackJobsCounted()
    {
        discoveryWorker->saveFindings(
            QStringLiteral("employerboards"),
            { foundJob(QStringLiteral("1"), QStringLiteral("Nurse")),
              foundJob(QStringLiteral("2"), QStringLiteral("Qt Developer")),
              foundJob(QStringLiteral("3"), QStringLiteral("Qt Developer, Berlin"),
                       QStringLiteral("Berlin, Germany")) });

        ScoringProfile scoringProfile;
        scoringProfile.targetJobTitles = { QStringLiteral("Qt Developer") };
        scoringProfile.preferredWorkLocations = { QStringLiteral("Pittsburgh, PA") };

        QSignalSpy rowsSpy(discoveryWorker.get(), &DiscoveryWorker::rowsPrepared);
        DiscoveryRowRequest topProspectsRequest;
        topProspectsRequest.rowKind = DiscoveryRowRequest::RowKind::TopProspects;
        discoveryWorker->prepareRows(7, topProspectsRequest, scoringProfile);

        QCOMPARE(rowsSpy.count(), 1);
        QCOMPARE(rowsSpy.at(0).at(0).toInt(), 7);
        const DiscoveryRows topRows = rowsSpy.at(0).at(1).value<DiscoveryRows>();
        QCOMPARE(topRows.scoredJobPostings.count(), 2);
        QCOMPARE(topRows.scoredJobPostings.first().jobPosting.positionTitle,
                 QStringLiteral("Qt Developer"));
        QCOMPARE(topRows.jobCountOutsideSearchArea, 1);

        DiscoveryRowRequest outsideRequest;
        outsideRequest.rowKind = DiscoveryRowRequest::RowKind::OutsideSearchArea;
        discoveryWorker->prepareRows(8, outsideRequest, scoringProfile);
        const DiscoveryRows outsideRows = rowsSpy.at(1).at(1).value<DiscoveryRows>();
        QCOMPARE(outsideRows.scoredJobPostings.count(), 1);
        QCOMPARE(outsideRows.scoredJobPostings.first().jobPosting.locationText,
                 QStringLiteral("Berlin, Germany"));
    }

    void removedJobsAndBoardJobsAreLeftOff()
    {
        // Three jobs from one board, and a second copy of the Nurse job from
        // another site.
        discoveryWorker->saveFindings(
            QStringLiteral("employerboards"),
            { foundJob(QStringLiteral("1"), QStringLiteral("Nurse")),
              foundJob(QStringLiteral("2"), QStringLiteral("Qt Developer")),
              foundJob(QStringLiteral("3"), QStringLiteral("C++ Engineer")) });
        JobPosting nurseOnAnotherSite = foundJob(QStringLiteral("9"), QStringLiteral("Nurse"));
        nurseOnAnotherSite.postingSource = QStringLiteral("remotive");
        discoveryWorker->saveFindings(QStringLiteral("remotive"), { nurseOnAnotherSite });

        JobPostingRepository postingRepository(*mainDatabase);
        qint64 nurseId = 0;
        qint64 qtDeveloperId = 0;
        for (const JobPosting &stored : postingRepository.loadAllJobPostings()) {
            if (stored.postingSource == QStringLiteral("employerboards")
                    && stored.positionTitle == QStringLiteral("Nurse")) {
                nurseId = stored.jobPostingId;
            }
            if (stored.positionTitle == QStringLiteral("Qt Developer")) {
                qtDeveloperId = stored.jobPostingId;
            }
        }
        QVERIFY(nurseId != 0 && qtDeveloperId != 0);

        // The user removes the Nurse job, and the Qt Developer job goes on
        // the board.
        QVERIFY(postingRepository.markRemovedByUser(nurseId));
        QSqlQuery cardQuery(mainDatabase->connection());
        QVERIFY(cardQuery.exec(QStringLiteral(
            "INSERT INTO jobApplication (jobPostingId, pipelineStage, targetedTimestamp) "
            "VALUES (%1, 'closed', '2026-11-18T12:00:00')").arg(qtDeveloperId)));

        QSignalSpy rowsSpy(discoveryWorker.get(), &DiscoveryWorker::rowsPrepared);
        const ScoringProfile scoringProfile;

        // Top Prospects shows only the C++ Engineer job. Both copies of the
        // Nurse job are gone, and the board job is left off.
        DiscoveryRowRequest topProspectsRequest;
        topProspectsRequest.rowKind = DiscoveryRowRequest::RowKind::TopProspects;
        discoveryWorker->prepareRows(1, topProspectsRequest, scoringProfile);
        const DiscoveryRows topRows = rowsSpy.at(0).at(1).value<DiscoveryRows>();
        QCOMPARE(topRows.scoredJobPostings.count(), 1);
        QCOMPARE(topRows.scoredJobPostings.first().jobPosting.positionTitle,
                 QStringLiteral("C++ Engineer"));

        // The board's own tab still shows the board job, but not the removed one.
        DiscoveryRowRequest oneSourceRequest;
        oneSourceRequest.rowKind = DiscoveryRowRequest::RowKind::OneSource;
        oneSourceRequest.sourceStorageName = QStringLiteral("employerboards");
        discoveryWorker->prepareRows(2, oneSourceRequest, scoringProfile);
        const DiscoveryRows sourceRows = rowsSpy.at(1).at(1).value<DiscoveryRows>();
        QCOMPARE(sourceRows.scoredJobPostings.count(), 2);
        for (const ScoredJobPosting &row : sourceRows.scoredJobPostings) {
            QVERIFY(row.jobPosting.positionTitle != QStringLiteral("Nurse"));
        }

        // The other site's tab does not show its copy of the removed job.
        oneSourceRequest.sourceStorageName = QStringLiteral("remotive");
        discoveryWorker->prepareRows(3, oneSourceRequest, scoringProfile);
        QCOMPARE(rowsSpy.at(2).at(1).value<DiscoveryRows>().scoredJobPostings.count(), 0);

        // A sweep that finds the Nurse job again does not bring it back.
        discoveryWorker->saveFindings(QStringLiteral("employerboards"),
                                      { foundJob(QStringLiteral("1"), QStringLiteral("Nurse")) });
        discoveryWorker->prepareRows(4, topProspectsRequest, scoringProfile);
        QCOMPARE(rowsSpy.at(3).at(1).value<DiscoveryRows>().scoredJobPostings.count(), 1);
    }
};

QTEST_GUILESS_MAIN(DiscoveryWorkerTest)
#include "tst_DiscoveryWorker.moc"
