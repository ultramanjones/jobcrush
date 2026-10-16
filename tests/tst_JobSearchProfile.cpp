#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "IsolatedTestSettings.h"
#include "modelview/jobscout/JobSearchProfile.h"

// Tests for JobSearchProfile: what the user is looking for, and how it is
// cleaned up and saved.
class TestJobSearchProfile : public QObject {
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

    void termsAreTrimmedAndDuplicatesDropped()
    {
        JobSearchProfile profile;
        profile.setSkillKeywords({ QStringLiteral("  Qt "), QStringLiteral("qt"),
                                   QStringLiteral(""), QStringLiteral("C++") });
        QCOMPARE(profile.skillKeywords(),
                 QStringList({ QStringLiteral("Qt"), QStringLiteral("C++") }));
    }

    void aLocationWithACommaStaysWhole()
    {
        JobSearchProfile profile;
        profile.addPreferredWorkLocation(QStringLiteral("Pittsburgh, PA"));
        QCOMPARE(profile.preferredWorkLocations(),
                 QStringList({ QStringLiteral("Pittsburgh, PA") }));
    }

    void addingTheSameLocationTwiceDoesNothing()
    {
        JobSearchProfile profile;
        profile.addPreferredWorkLocation(QStringLiteral("Pittsburgh, PA"));
        profile.addPreferredWorkLocation(QStringLiteral("pittsburgh, pa"));
        QCOMPARE(profile.preferredWorkLocations().count(), 1);
    }

    void removingALocationOutOfRangeDoesNothing()
    {
        JobSearchProfile profile;
        profile.addPreferredWorkLocation(QStringLiteral("Remote"));
        profile.removePreferredWorkLocationAt(5);
        profile.removePreferredWorkLocationAt(-1);
        QCOMPARE(profile.preferredWorkLocations().count(), 1);
    }

    void aNegativeSalaryIsReadAsNoFloor()
    {
        JobSearchProfile profile;
        profile.setMinimumAcceptableSalary(-50000);
        QCOMPARE(profile.minimumAcceptableSalary(), 0);
    }

    void oneTitleOrOneSkillIsEnoughToRank()
    {
        JobSearchProfile profile;
        QVERIFY(!profile.hasEnoughToRankBy());
        profile.setSkillKeywords({ QStringLiteral("QML") });
        QVERIFY(profile.hasEnoughToRankBy());
    }

    void matchableTermsAreLowercaseWithNoRepeats()
    {
        JobSearchProfile profile;
        profile.setTargetJobTitles({ QStringLiteral("Qt Developer") });
        profile.setSkillKeywords({ QStringLiteral("QML"), QStringLiteral("qt developer") });
        QCOMPARE(profile.matchableTermsLowercased(),
                 QStringList({ QStringLiteral("qt developer"), QStringLiteral("qml") }));
    }

    void settingTheSameValueSendsNoSignal()
    {
        JobSearchProfile profile;
        profile.setRemoteRolesOnly(true);
        QSignalSpy changeSpy(&profile, &JobSearchProfile::searchProfileChanged);
        profile.setRemoteRolesOnly(true);
        QCOMPARE(changeSpy.count(), 0);
        profile.setRemoteRolesOnly(false);
        QCOMPARE(changeSpy.count(), 1);
    }

    void whatIsSavedComesBack()
    {
        {
            JobSearchProfile savingProfile;
            savingProfile.setTargetJobTitles({ QStringLiteral("Qt Developer") });
            savingProfile.setSkillKeywords({ QStringLiteral("C++"), QStringLiteral("QML") });
            savingProfile.addPreferredWorkLocation(QStringLiteral("Pittsburgh, PA"));
            savingProfile.setRemoteRolesOnly(true);
            savingProfile.setMinimumAcceptableSalary(120000);
        }
        JobSearchProfile loadingProfile;
        loadingProfile.loadFromSettings();
        QCOMPARE(loadingProfile.targetJobTitles(), QStringList({ QStringLiteral("Qt Developer") }));
        QCOMPARE(loadingProfile.skillKeywords(),
                 QStringList({ QStringLiteral("C++"), QStringLiteral("QML") }));
        QCOMPARE(loadingProfile.preferredWorkLocations(),
                 QStringList({ QStringLiteral("Pittsburgh, PA") }));
        QVERIFY(loadingProfile.remoteRolesOnly());
        QCOMPARE(loadingProfile.minimumAcceptableSalary(), 120000);
    }

    void theOldSingleLocationBoxIsCarriedOver()
    {
        QSettings().setValue(QStringLiteral("jobSearchProfile/preferredLocationText"),
                             QStringLiteral("Pittsburgh, Remote"));
        JobSearchProfile profile;
        profile.loadFromSettings();
        QCOMPARE(profile.preferredWorkLocations(),
                 QStringList({ QStringLiteral("Pittsburgh"), QStringLiteral("Remote") }));
    }

private:
    QTemporaryDir settingsFolder;
};

QTEST_GUILESS_MAIN(TestJobSearchProfile)
#include "tst_JobSearchProfile.moc"
