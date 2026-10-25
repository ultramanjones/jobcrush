#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include "logic/jobscout/AtsBoardDetector.h"
#include "logic/jobscout/WorkdayBoardSource.h"

// Tests for reading Workday.
//
// The JSON below is cut down from real answers from Northrop Grumman's
// Workday site on 11 Oct 2026. No network is used. If Workday changes its
// answer shape, these still pass; the first pasted link will show the change.
class TestWorkdayBoardSource : public QObject {
    Q_OBJECT

private:
    // One real job link, with and without the language part ("en-US"), and
    // the tenant and job path it should be read into.
    const QString jobLinkWithLanguage = QStringLiteral(
        "https://ngc.wd1.myworkdayjobs.com/en-US/Northrop_Grumman_External_Site/job/"
        "United-States-Georgia-Warner-Robins/Embedded---Principal-Software-Engineer_R10252946");
    const QString jobLinkWithoutLanguage = QStringLiteral(
        "https://ngc.wd1.myworkdayjobs.com/Northrop_Grumman_External_Site/job/"
        "United-States-Georgia-Warner-Robins/Embedded---Principal-Software-Engineer_R10252946");
    const QString expectedTenant = QStringLiteral("ngc.wd1/Northrop_Grumman_External_Site");
    const QString expectedJobPath = QStringLiteral(
        "job/United-States-Georgia-Warner-Robins/Embedded---Principal-Software-Engineer_R10252946");

private slots:
    void aJobLinkIsReadIntoItsThreeParts()
    {
        const AtsBoardIdentity identity = AtsBoardDetector().identify(jobLinkWithLanguage);
        QCOMPARE(identity.boardName, AtsBoardName::Workday);
        QCOMPARE(identity.tenant, expectedTenant);
        QCOMPARE(identity.jobId, expectedJobPath);
    }

    void theLanguageInTheLinkDoesNotChangeTheJob()
    {
        AtsBoardDetector detector;
        QCOMPARE(detector.identify(jobLinkWithoutLanguage).asKey(),
                 detector.identify(jobLinkWithLanguage).asKey());
    }

    void aSiteLinkNamesTheEmployerButNoJob()
    {
        const AtsBoardIdentity identity = AtsBoardDetector().identify(
            QStringLiteral("https://l3harris.wd5.myworkdayjobs.com/en-US/L3HarrisCareers"));
        QCOMPARE(identity.tenant, QStringLiteral("l3harris.wd5/L3HarrisCareers"));
        QVERIFY(identity.isKnown());
        QVERIFY(!identity.namesOneJob());
    }

    void theTenantSplitsBackApart()
    {
        WorkdayBoardSource::WorkdaySite workdaySite;
        QVERIFY(WorkdayBoardSource::readTenant(expectedTenant, workdaySite));
        QCOMPARE(workdaySite.siteHost, QStringLiteral("ngc.wd1.myworkdayjobs.com"));
        QCOMPARE(workdaySite.cxsTenant, QStringLiteral("ngc"));
        QCOMPARE(workdaySite.siteName, QStringLiteral("Northrop_Grumman_External_Site"));
        QCOMPARE(workdaySite.publicSitePath, QStringLiteral("Northrop_Grumman_External_Site"));
        QVERIFY(!WorkdayBoardSource::readTenant(QStringLiteral("acme"), workdaySite));
    }

    // The other Workday host keeps the account and the site in the path.
    // Link shape checked live on 2026-10-23.
    void aMyWorkdaySiteLinkIsReadToo()
    {
        const AtsBoardIdentity identity = AtsBoardDetector().identify(QStringLiteral(
            "https://wd5.myworkdaysite.com/en-US/recruiting/uw/UWHires/job/Seattle-WA/"
            "Research-Specimen-Processing-Technician_REQ-0000136398"));
        QCOMPARE(identity.boardName, AtsBoardName::Workday);
        QCOMPARE(identity.tenant, QStringLiteral("wd5.myworkdaysite/uw/UWHires"));
        QCOMPARE(identity.jobId, QStringLiteral(
            "job/Seattle-WA/Research-Specimen-Processing-Technician_REQ-0000136398"));

        WorkdayBoardSource::WorkdaySite workdaySite;
        QVERIFY(WorkdayBoardSource::readTenant(identity.tenant, workdaySite));
        QCOMPARE(workdaySite.siteHost, QStringLiteral("wd5.myworkdaysite.com"));
        QCOMPARE(workdaySite.cxsTenant, QStringLiteral("uw"));
        QCOMPARE(workdaySite.siteName, QStringLiteral("UWHires"));
        QCOMPARE(workdaySite.publicSitePath, QStringLiteral("recruiting/uw/UWHires"));
    }

    void theCompanyNameComesFromTheSiteName()
    {
        QCOMPARE(WorkdayBoardSource::companyNameFromSiteName(
                     QStringLiteral("Northrop_Grumman_External_Site")),
                 QStringLiteral("Northrop Grumman"));
        QCOMPARE(WorkdayBoardSource::companyNameFromSiteName(QStringLiteral("L3HarrisCareers")),
                 QStringLiteral("L3Harris"));
    }

    void postedOnTextIsReadAsADate()
    {
        const QDate today(2026, 10, 11);
        QCOMPARE(WorkdayBoardSource::postedDateFromDisplayText(QStringLiteral("Posted Today"), today),
                 today);
        QCOMPARE(WorkdayBoardSource::postedDateFromDisplayText(
                     QStringLiteral("Posted Yesterday"), today), QDate(2026, 10, 10));
        QCOMPARE(WorkdayBoardSource::postedDateFromDisplayText(
                     QStringLiteral("Posted 14 Days Ago"), today), QDate(2026, 9, 27));
        QCOMPARE(WorkdayBoardSource::postedDateFromDisplayText(
                     QStringLiteral("Posted 30+ Days Ago"), today), QDate(2026, 9, 11));
        QVERIFY(!WorkdayBoardSource::postedDateFromDisplayText(
                     QStringLiteral("Recently"), today).isValid());
    }

    void aSearchRowHasTheSameIdAsTheLink()
    {
        // Built by hand rather than from a raw string: older versions of
        // moc misread raw string literals and skip the whole class.
        QJsonObject searchEntry;
        searchEntry.insert(QStringLiteral("title"),
                           QStringLiteral("Embedded / Principal Software Engineer"));
        searchEntry.insert(QStringLiteral("externalPath"), QLatin1Char('/') + expectedJobPath);
        searchEntry.insert(QStringLiteral("timeType"), QStringLiteral("Full time"));
        searchEntry.insert(QStringLiteral("locationsText"),
                           QStringLiteral("United States-Georgia-Warner Robins"));
        searchEntry.insert(QStringLiteral("postedOn"), QStringLiteral("Posted 14 Days Ago"));
        searchEntry.insert(QStringLiteral("bulletFields"),
                           QJsonArray({ QStringLiteral("R10252946") }));

        const JobPosting posting =
            WorkdayBoardSource::postingFromSearchEntry(searchEntry, expectedTenant);
        QCOMPARE(posting.positionTitle, QStringLiteral("Embedded / Principal Software Engineer"));
        QCOMPARE(posting.externalSourceId, expectedJobPath);
        QCOMPARE(posting.companyName, QStringLiteral("Northrop Grumman"));
        QCOMPARE(posting.postingSource, AtsBoardName::Workday);
        QVERIFY(posting.fullDescriptionText.isEmpty());
    }

    void theOneJobAnswerIsReadInFull()
    {
        QJsonObject jobInfo;
        jobInfo.insert(QStringLiteral("title"),
                       QStringLiteral("Embedded / Principal Software Engineer"));
        jobInfo.insert(QStringLiteral("jobDescription"),
                       QStringLiteral("<p>RELOCATION ASSISTANCE: maybe</p><p>Build <b>Qt</b> UIs.</p>"));
        jobInfo.insert(QStringLiteral("location"), QStringLiteral("United States-Virginia-McLean"));
        jobInfo.insert(QStringLiteral("additionalLocations"),
                       QJsonArray({ QStringLiteral("United States-Utah-Roy"),
                                    QStringLiteral("United States-Virginia-McLean") }));
        jobInfo.insert(QStringLiteral("postedOn"), QStringLiteral("Posted 14 Days Ago"));
        jobInfo.insert(QStringLiteral("startDate"), QStringLiteral("2026-09-27"));
        jobInfo.insert(QStringLiteral("timeType"), QStringLiteral("Full time"));
        jobInfo.insert(QStringLiteral("jobReqId"), QStringLiteral("R10252946"));
        jobInfo.insert(QStringLiteral("externalUrl"), jobLinkWithoutLanguage);

        QJsonObject hiringOrganization;
        hiringOrganization.insert(QStringLiteral("name"),
                                  QStringLiteral("0090 CORP-Corporate Office"));

        QJsonObject jobDetail;
        jobDetail.insert(QStringLiteral("jobPostingInfo"), jobInfo);
        jobDetail.insert(QStringLiteral("hiringOrganization"), hiringOrganization);

        const JobPosting posting = WorkdayBoardSource::postingFromJobDetail(
            jobDetail, expectedTenant, QLatin1Char('/') + expectedJobPath);

        // The hiring office name ("0090 CORP-Corporate Office") must never
        // become the company name.
        QCOMPARE(posting.companyName, QStringLiteral("Northrop Grumman"));
        QCOMPARE(posting.externalSourceId, expectedJobPath);
        QCOMPARE(posting.locationText,
                 QStringLiteral("United States-Virginia-McLean; United States-Utah-Roy"));
        QCOMPARE(posting.postedTimestamp.date(), QDate(2026, 9, 27));
        QVERIFY(posting.fullDescriptionText.contains(QStringLiteral("Build Qt UIs.")));
        QVERIFY(!posting.fullDescriptionText.contains(QLatin1Char('<')));

        // The saved link must point back at the same job as the pasted one.
        QCOMPARE(AtsBoardDetector().identify(posting.sourceUrl).asKey(),
                 AtsBoardDetector().identify(jobLinkWithLanguage).asKey());
    }
};

QTEST_GUILESS_MAIN(TestWorkdayBoardSource)
#include "tst_WorkdayBoardSource.moc"
