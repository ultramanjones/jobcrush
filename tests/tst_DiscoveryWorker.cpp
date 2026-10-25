#include <QtTest>

#include <QSignalSpy>
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
};

QTEST_GUILESS_MAIN(DiscoveryWorkerTest)
#include "tst_DiscoveryWorker.moc"
