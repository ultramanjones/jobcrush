#include <QtTest>

#include <QSignalSpy>
#include <QTimer>

#include "logic/jobscout/AtsBoardDetector.h"
#include "logic/jobscout/EmployerBoardProvider.h"
#include "logic/jobscout/JobScoutReply.h"
#include "logic/pipelines/PostingGoneConfirmer.h"

namespace {

// What the fake board says to each question.
enum class FakeAnswer { Found, NotFound, Fails };

// A board that answers from memory instead of the network.
class FakeBoard : public EmployerBoardProvider {
public:
    FakeAnswer jobPageAnswer = FakeAnswer::NotFound;
    FakeAnswer boardListAnswer = FakeAnswer::Found;
    QList<JobPosting> listedJobs;
    bool boardListStopsAtThePageLimit = false;

    // When set, only a search for these words finds listedJobs.
    QString onlySearchThatFindsTheJobs;
    QStringList searchesAsked;

    QString boardName() const override { return AtsBoardName::Greenhouse; }

    JobScoutReply *fetchEveryJobForEmployer(const QString &, QObject *replyParent) override
    {
        JobScoutReply *scoutReply = answerLater(boardListAnswer, listedJobs, replyParent);
        scoutReply->setStoppedAtThePageLimit(boardListStopsAtThePageLimit);
        return scoutReply;
    }

    JobScoutReply *fetchJobsLikeTitle(const QString &tenant, const QString &searchWords,
                                      QObject *replyParent) override
    {
        searchesAsked.append(searchWords);
        if (!onlySearchThatFindsTheJobs.isEmpty() && searchWords != onlySearchThatFindsTheJobs) {
            return answerLater(FakeAnswer::NotFound, {}, replyParent);
        }
        return fetchEveryJobForEmployer(tenant, replyParent);
    }

    JobScoutReply *fetchOneJob(const AtsBoardIdentity &, QObject *replyParent) override
    {
        JobPosting thePosting;
        thePosting.positionTitle = QStringLiteral("Qt Developer");
        return answerLater(jobPageAnswer, { thePosting }, replyParent);
    }

private:
    // Makes a reply that answers on the next turn of the event loop, the way
    // a network reply would, rather than before the caller is listening.
    static JobScoutReply *answerLater(FakeAnswer fakeAnswer, const QList<JobPosting> &postings,
                                      QObject *replyParent)
    {
        JobScoutReply *scoutReply = new JobScoutReply(replyParent);
        QTimer::singleShot(0, scoutReply, [scoutReply, fakeAnswer, postings]() {
            switch (fakeAnswer) {
            case FakeAnswer::Found:    scoutReply->markFinished(postings); break;
            case FakeAnswer::NotFound: scoutReply->markFinished({});       break;
            case FakeAnswer::Fails:    scoutReply->markFailed(QStringLiteral("timed out")); break;
            }
        });
        return scoutReply;
    }
};

// How long one check may take before the test gives up on it. The fake
// board answers at once, so this only matters when something is broken.
constexpr int longestCheckWaitMilliseconds = 2000;

// A job as it appears in the company's list, with a Greenhouse link.
JobPosting listedJob(const QString &jobId, const QString &positionTitle)
{
    JobPosting jobPosting;
    jobPosting.positionTitle = positionTitle;
    jobPosting.sourceUrl = QStringLiteral("https://boards.greenhouse.io/acme/jobs/%1").arg(jobId);
    return jobPosting;
}

} // namespace

// Tests for PostingGoneConfirmer: a job is called gone only when the job's
// own page AND the company's list both say it is not there. A fake board
// stands in for the network.
class PostingGoneConfirmerTest : public QObject {
    Q_OBJECT

private:
    // Runs one check of one job against the fake board and returns its
    // answer. A check that never answers counts as CouldNotCheck.
    ListingAnswer answerFor(FakeBoard &fakeBoard, const QString &positionTitle,
                            const QString &jobLink =
                                QStringLiteral("https://boards.greenhouse.io/acme/jobs/4055"))
    {
        PostingGoneConfirmer goneConfirmer;
        const AtsBoardIdentity identity = AtsBoardDetector().identify(jobLink);
        goneConfirmer.replaceBoardReader(identity.boardName, &fakeBoard);

        QObject replyParent;
        ListingCheckReply *checkReply = goneConfirmer.checkOneJob(identity, positionTitle,
                                                                  &replyParent);
        QSignalSpy answeredSpy(checkReply, &ListingCheckReply::answered);
        if (!answeredSpy.wait(longestCheckWaitMilliseconds)) {
            return ListingAnswer::CouldNotCheck;
        }
        return answeredSpy.at(0).at(0).value<ListingAnswer>();
    }

private slots:
    void aJobWhosePageIsThereIsStillListed()
    {
        FakeBoard fakeBoard;
        fakeBoard.jobPageAnswer = FakeAnswer::Found;
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Qt Developer")), ListingAnswer::StillListed);
    }

    void notFoundOnItsPageAndNotInTheListIsGone()
    {
        FakeBoard fakeBoard;
        fakeBoard.listedJobs = { listedJob(QStringLiteral("9999"), QStringLiteral("Nurse")) };
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Qt Developer")), ListingAnswer::Gone);
    }

    // The case behind the false alarms: the page said "not found", but the
    // company still lists the job.
    void notFoundOnItsPageButStillInTheListIsStillListed()
    {
        FakeBoard fakeBoard;
        fakeBoard.listedJobs = { listedJob(QStringLiteral("4055"), QStringLiteral("Qt Developer")) };
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Qt Developer")), ListingAnswer::StillListed);
    }

    void aJobWithTheSameTitleCountsAsStillListed()
    {
        FakeBoard fakeBoard;
        fakeBoard.listedJobs = { listedJob(QStringLiteral("7777"), QStringLiteral("Qt developer (Remote)")) };
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Qt Developer - Remote")),
                 ListingAnswer::StillListed);
    }

    void aBoardListThatFailsMeansCouldNotCheck()
    {
        FakeBoard fakeBoard;
        fakeBoard.boardListAnswer = FakeAnswer::Fails;
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Qt Developer")), ListingAnswer::CouldNotCheck);
    }

    // The company has more jobs than one check reads, so the job may be
    // past the stop.
    void aListCutShortAtThePageLimitMeansCouldNotCheck()
    {
        FakeBoard fakeBoard;
        fakeBoard.listedJobs = { listedJob(QStringLiteral("9999"), QStringLiteral("Nurse")) };
        fakeBoard.boardListStopsAtThePageLimit = true;
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Qt Developer")), ListingAnswer::CouldNotCheck);
    }

    // The employer edited the title, so the old link and the old title both
    // miss. The requisition number still finds it.
    void workdayIsAlsoSearchedForTheRequisitionNumber()
    {
        JobPosting retitledJob;
        retitledJob.positionTitle = QStringLiteral("Lead Software Engineer");
        retitledJob.sourceUrl = QStringLiteral(
            "https://ngc.wd1.myworkdayjobs.com/Northrop_Grumman_External_Site/job/Remote/"
            "Lead-Software-Engineer_R10252946");

        FakeBoard fakeBoard;
        fakeBoard.listedJobs = { retitledJob };
        fakeBoard.onlySearchThatFindsTheJobs = QStringLiteral("R10252946");
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Principal Software Engineer"),
                           QStringLiteral("https://ngc.wd1.myworkdayjobs.com/"
                                          "Northrop_Grumman_External_Site/job/Remote/"
                                          "Principal-Software-Engineer_R10252946")),
                 ListingAnswer::StillListed);
        QCOMPARE(fakeBoard.searchesAsked,
                 QStringList({ QStringLiteral("Principal Software Engineer"),
                               QStringLiteral("R10252946") }));
    }

    void aJobPageThatFailsMeansCouldNotCheck()
    {
        FakeBoard fakeBoard;
        fakeBoard.jobPageAnswer = FakeAnswer::Fails;
        QCOMPARE(answerFor(fakeBoard, QStringLiteral("Qt Developer")), ListingAnswer::CouldNotCheck);
    }
};

QTEST_GUILESS_MAIN(PostingGoneConfirmerTest)
#include "tst_PostingGoneConfirmer.moc"
