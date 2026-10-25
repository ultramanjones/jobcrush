#include <QTest>

#include "logic/jobscout/AtsBoardDetector.h"
#include "logic/jobscout/CareerPreset.h"
#include "logic/jobscout/CompanyList.h"

// Tests for the shipped data files: the company list and the career preset.
// They read the real files in data/, so a typo in a file fails here instead
// of in front of a user.
class CompanyListTest : public QObject {
    Q_OBJECT

    // Floors, not exact counts, so adding companies never breaks the test.
    // Falling below them means the file lost entries by mistake.
    static constexpr int fewestCompaniesExpected = 50;
    static constexpr int fewestFollowableCompaniesExpected = 20;

private slots:
    void theShippedListReadsAndEveryLinkIsFollowable()
    {
        CompanyList companyList;
        QString problemText;
        QVERIFY2(CompanyList::readFromFile(
                     QStringLiteral(JOBCRUSH_SOURCE_DIR "/data/company-lists/cpp-qt-medtech-tech.json"),
                     companyList, problemText),
                 qPrintable(problemText));
        QVERIFY(companyList.companies.count() >= fewestCompaniesExpected);

        // Every company with a follow link must point at a hiring system Job
        // Crush can read, and the file must name that same system.
        int followable = 0;
        for (const CompanyListEntry &entry : companyList.companies) {
            if (entry.followLink.isEmpty()) {
                continue;
            }
            ++followable;
            const AtsBoardIdentity identity = AtsBoardDetector().identify(entry.followLink);
            QVERIFY2(identity.isKnown(), qPrintable(entry.companyName + " " + entry.followLink));
            QVERIFY2(identity.boardName == entry.hiringSystem,
                     qPrintable(entry.companyName + " says " + entry.hiringSystem));
        }
        QVERIFY(followable >= fewestFollowableCompaniesExpected);
    }

    // A company is found even when the name's case or wording differs a
    // little from the list, and a company not on the list is not found.
    void companiesAreFoundLoosely()
    {
        CompanyList companyList;
        QString problemText;
        QVERIFY(CompanyList::readFromFile(
            QStringLiteral(JOBCRUSH_SOURCE_DIR "/data/company-lists/cpp-qt-medtech-tech.json"),
            companyList, problemText));
        CompanyListEntry entry;
        QVERIFY(companyList.findCompany(QStringLiteral("collins aerospace"), entry));
        QCOMPARE(entry.hiringSystem, QStringLiteral("workday"));
        QVERIFY(companyList.findCompany(QStringLiteral("Bosch Group"), entry));
        QVERIFY(companyList.findCompany(QStringLiteral("Stryker Corporation"), entry));
        QVERIFY(!companyList.findCompany(QStringLiteral("Acme Widgets"), entry));
    }

    void theShippedPresetReads()
    {
        CareerPreset preset;
        QString problemText;
        QVERIFY2(CareerPreset::readFromFile(
                     QStringLiteral(JOBCRUSH_SOURCE_DIR "/data/career-presets/cpp-qt.json"),
                     preset, problemText),
                 qPrintable(problemText));
        QVERIFY(preset.jobTitles.contains(QStringLiteral("Qt Developer")));
        QVERIFY(preset.strongSkills.contains(QStringLiteral("QML")));
    }
};

QTEST_GUILESS_MAIN(CompanyListTest)
#include "tst_CompanyList.moc"
