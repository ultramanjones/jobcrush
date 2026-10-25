#include <QDateTime>
#include <QTemporaryDir>
#include <QTest>

#include "IsolatedTestSettings.h"
#include "logic/jobscout/JobSearchProfile.h"
#include "logic/jobscout/ClearanceDetector.h"
#include "logic/jobscout/ProspectScorer.h"

namespace {

// A posting with only the fields a scoring test cares about.
JobPosting postingWith(const QString &positionTitle,
                       const QString &descriptionText = QString(),
                       const QString &locationText = QString(),
                       bool isRemoteRole = false)
{
    JobPosting posting;
    posting.positionTitle = positionTitle;
    posting.fullDescriptionText = descriptionText;
    posting.locationText = locationText;
    posting.isRemoteRole = isRemoteRole;
    return posting;
}

} // namespace

// Tests for ProspectScorer, the arithmetic behind Top Prospects. No network,
// no AI, no window. If any of these fail, the ranking on Discoveries is wrong.
class TestProspectScorer : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(settingsFolder.isValid());
        useIsolatedTestSettings(settingsFolder);
    }

    void init()
    {
        QSettings().clear();
    }

    void aDirectTitleMatchBeatsAPartialOne()
    {
        JobSearchProfile profile;
        profile.setTargetJobTitles({ QStringLiteral("Qt Developer") });
        const ProspectScorer scorer(profile);

        const int directScore =
            scorer.scoreJobPosting(postingWith(QStringLiteral("Senior Qt Developer")))
                .matchScoreOutOfOneHundred;
        const int partialScore =
            scorer.scoreJobPosting(postingWith(QStringLiteral("Qt Software Engineer")))
                .matchScoreOutOfOneHundred;
        QCOMPARE(directScore, 40); // all of the title points
        QVERIFY(partialScore > 0);
        QVERIFY(directScore > partialScore);
    }

    void aGenericRoleWordAloneEarnsAlmostNothing()
    {
        JobSearchProfile profile;
        profile.setTargetJobTitles({ QStringLiteral("Qt Developer") });
        const ProspectScorer scorer(profile);

        const int qtScore =
            scorer.scoreJobPosting(postingWith(QStringLiteral("Qt Engineer")))
                .matchScoreOutOfOneHundred;
        const int javascriptScore =
            scorer.scoreJobPosting(postingWith(QStringLiteral("JavaScript Developer")))
                .matchScoreOutOfOneHundred;
        QVERIFY(qtScore > javascriptScore);
        QVERIFY(javascriptScore < 8);
    }

    void javaDoesNotMatchJavaScript()
    {
        JobSearchProfile profile;
        profile.setSkillKeywords({ QStringLiteral("Java") });
        const ProspectScorer scorer(profile);

        const ProspectMatchResult result = scorer.scoreJobPosting(
            postingWith(QStringLiteral("Web Developer"),
                        QStringLiteral("We use JavaScript and TypeScript.")));
        QCOMPARE(result.matchScoreOutOfOneHundred, 0);
    }

    // The first "java" here is inside "JavaScript". The second one is a whole
    // word and must still be found.
    void javaIsFoundAfterJavaScript()
    {
        JobSearchProfile profile;
        profile.setSkillKeywords({ QStringLiteral("Java") });
        const ProspectScorer scorer(profile);

        const ProspectMatchResult result = scorer.scoreJobPosting(
            postingWith(QStringLiteral("Web Developer"),
                        QStringLiteral("We use JavaScript on the front and Java on the back.")));
        QCOMPARE(result.matchScoreOutOfOneHundred, 35); // all of the skill points
    }

    void matchIsStrongFromSixtyAndFairFromThirty()
    {
        QCOMPARE(ProspectScorer::matchStrengthFor(60), ProspectMatchStrength::Strong);
        QCOMPARE(ProspectScorer::matchStrengthFor(59), ProspectMatchStrength::Fair);
        QCOMPARE(ProspectScorer::matchStrengthFor(30), ProspectMatchStrength::Fair);
        QCOMPARE(ProspectScorer::matchStrengthFor(29), ProspectMatchStrength::Weak);
    }

    void cPlusPlusMatchesEvenWithSymbols()
    {
        JobSearchProfile profile;
        profile.setSkillKeywords({ QStringLiteral("C++"), QStringLiteral("QML") });
        const ProspectScorer scorer(profile);

        const ProspectMatchResult result = scorer.scoreJobPosting(
            postingWith(QStringLiteral("Engineer"),
                        QStringLiteral("Strong C++, some QML.")));
        QCOMPARE(result.matchScoreOutOfOneHundred, 35); // 2 of 2 skills: all 35 points
        QVERIFY(result.matchReasons.join(QLatin1Char(' ')).contains(QStringLiteral("2 of your 2")));
    }

    void aSkillRepeatedCountsOnce()
    {
        JobSearchProfile profile;
        profile.setSkillKeywords({ QStringLiteral("Qt"), QStringLiteral("Python") });
        const ProspectScorer scorer(profile);

        const ProspectMatchResult result = scorer.scoreJobPosting(
            postingWith(QStringLiteral("Engineer"), QStringLiteral("Qt Qt Qt Qt Qt Qt")));
        QCOMPARE(result.matchScoreOutOfOneHundred, 17); // 1 of 2 skills: 35 * 1 / 2, rounded down
    }

    void aStateCodeMatchesOnlyAsAWholeWord()
    {
        JobSearchProfile profile;
        profile.addPreferredWorkLocation(QStringLiteral("PA"));
        const ProspectScorer scorer(profile);

        QCOMPARE(scorer.matchedWorkLocationFor(
                     postingWith(QStringLiteral("x"), {}, QStringLiteral("Pittsburgh, PA"))),
                 QStringLiteral("PA"));
        QVERIFY(scorer.matchedWorkLocationFor(
                    postingWith(QStringLiteral("x"), {}, QStringLiteral("Palo Alto, CA")))
                    .isEmpty());
    }

    void remoteAndPlacelessJobsAreAlwaysInsideTheSearchArea()
    {
        JobSearchProfile profile;
        profile.addPreferredWorkLocation(QStringLiteral("Pittsburgh"));
        const ProspectScorer scorer(profile);

        QVERIFY(scorer.jobPostingIsInsideSearchArea(
            postingWith(QStringLiteral("x"), {}, QStringLiteral("Berlin"), true)));
        QVERIFY(scorer.jobPostingIsInsideSearchArea(postingWith(QStringLiteral("x"))));
        QVERIFY(!scorer.jobPostingIsInsideSearchArea(
            postingWith(QStringLiteral("x"), {}, QStringLiteral("Houston, TX"))));
    }

    void noPlacesNamedMeansNothingIsFiltered()
    {
        JobSearchProfile profile;
        const ProspectScorer scorer(profile);
        QVERIFY(scorer.jobPostingIsInsideSearchArea(
            postingWith(QStringLiteral("x"), {}, QStringLiteral("Houston, TX"))));
    }

    void remoteOnlyHalvesAnOnSiteJobButKeepsIt()
    {
        JobSearchProfile profile;
        profile.setTargetJobTitles({ QStringLiteral("Qt Developer") });
        profile.setRemoteRolesOnly(true);
        const ProspectScorer scorer(profile);

        const ProspectMatchResult onSite = scorer.scoreJobPosting(
            postingWith(QStringLiteral("Qt Developer"), {}, QStringLiteral("Austin, TX")));
        QCOMPARE(onSite.matchScoreOutOfOneHundred, 20); // 40 title points, halved
        QVERIFY(onSite.matchReasons.contains(QStringLiteral("Not listed as remote")));
    }

    void payInThousandsIsReadAsThousands()
    {
        JobSearchProfile profile;
        profile.setMinimumAcceptableSalary(120000);
        const ProspectScorer scorer(profile);

        JobPosting posting = postingWith(QStringLiteral("x"));
        // The top of the range reaches the floor: all 5 pay points. A range
        // that stays under the floor earns none.
        posting.salaryText = QStringLiteral("$110k - $130k");
        QCOMPARE(scorer.scoreJobPosting(posting).matchScoreOutOfOneHundred, 5);

        posting.salaryText = QStringLiteral("$90,000 - $110,000");
        QCOMPARE(scorer.scoreJobPosting(posting).matchScoreOutOfOneHundred, 0);
    }

    void aFreshPostingScoresHigherThanAStaleOne()
    {
        JobSearchProfile profile;
        const ProspectScorer scorer(profile);

        JobPosting fresh = postingWith(QStringLiteral("x"));
        fresh.postedTimestamp = QDateTime::currentDateTime();
        JobPosting stale = postingWith(QStringLiteral("x"));
        stale.postedTimestamp = QDateTime::currentDateTime().addDays(-60);

        // Posted today: all 5 freshness points. Sixty days old: none.
        QCOMPARE(scorer.scoreJobPosting(fresh).matchScoreOutOfOneHundred, 5);
        QCOMPARE(scorer.scoreJobPosting(stale).matchScoreOutOfOneHundred, 0);
    }

    // ClearanceDetector lives beside the scorer and is tested here with it.
    void clearanceIsFlaggedFromTitleOrDescription()
    {
        JobPosting secretJob;
        secretJob.positionTitle = QStringLiteral("Embedded C++ Engineer");
        secretJob.fullDescriptionText =
            QStringLiteral("Must be able to obtain a DoD Secret clearance.");
        QVERIFY(ClearanceDetector::asksForClearance(secretJob));

        JobPosting tsJob;
        tsJob.positionTitle = QStringLiteral("Qt Developer (TS/SCI)");
        QVERIFY(ClearanceDetector::asksForClearance(tsJob));

        JobPosting plainJob;
        plainJob.positionTitle = QStringLiteral("Qt Developer");
        plainJob.fullDescriptionText = QStringLiteral("Build QML screens for robots.");
        QVERIFY(!ClearanceDetector::asksForClearance(plainJob));
    }

private:
    QTemporaryDir settingsFolder;
};

QTEST_GUILESS_MAIN(TestProspectScorer)
#include "tst_ProspectScorer.moc"
