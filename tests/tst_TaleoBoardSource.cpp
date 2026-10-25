#include <QtTest>

#include <QJsonArray>
#include <QJsonObject>

#include "logic/jobscout/AtsBoardDetector.h"
#include "logic/jobscout/TaleoBoardSource.h"

// Tests for TaleoBoardSource's reading. The sample answers below are cut down
// from live answers from hdr.taleo.net on 2026-10-23, with the same shapes
// and field names.
class TaleoBoardSourceTest : public QObject {
    Q_OBJECT

private slots:
    void aJobLinkIsReadIntoItsParts()
    {
        const AtsBoardIdentity identity = AtsBoardDetector().identify(QStringLiteral(
            "https://hdr.taleo.net/careersection/ex/jobdetail.ftl?job=193686&lang=en"));
        QCOMPARE(identity.boardName, AtsBoardName::Taleo);
        QCOMPARE(identity.tenant, QStringLiteral("hdr/ex"));
        QCOMPARE(identity.jobId, QStringLiteral("193686"));
        QVERIFY(identity.namesOneJob());
    }

    void aSearchPageLinkNamesTheSiteButNoJob()
    {
        const AtsBoardIdentity identity = AtsBoardDetector().identify(QStringLiteral(
            "https://cu.taleo.net/careersection/2/jobsearch.ftl?lang=en"));
        QCOMPARE(identity.tenant, QStringLiteral("cu/2"));
        QVERIFY(identity.isKnown());
        QVERIFY(!identity.namesOneJob());
    }

    void theTenantSplitsBackApart()
    {
        QString accountName;
        QString careerSectionName;
        QVERIFY(TaleoBoardSource::readTenant(QStringLiteral("hdr/ex"), accountName,
                                             careerSectionName));
        QCOMPARE(accountName, QStringLiteral("hdr"));
        QCOMPARE(careerSectionName, QStringLiteral("ex"));
        QVERIFY(!TaleoBoardSource::readTenant(QStringLiteral("hdr"), accountName,
                                              careerSectionName));
    }

    void thePortalNumberIsFoundEitherWay()
    {
        QCOMPARE(TaleoBoardSource::portalNumberFromSearchPage(
                     QStringLiteral("var config = { portalNo: '101430233', lang: 'en' };")),
                 QStringLiteral("101430233"));
        QCOMPARE(TaleoBoardSource::portalNumberFromSearchPage(QStringLiteral(
                     "<a href=\"/careersection/2/moresearch.ftl?lang=en&portal=8105120163\">")),
                 QStringLiteral("8105120163"));
        QVERIFY(TaleoBoardSource::portalNumberFromSearchPage(QStringLiteral("<html/>")).isEmpty());
    }

    void aSearchRowBecomesAPosting()
    {
        // Built with QJsonObject, not a raw string: Qt 6.4's moc stops reading
        // a test class that holds a raw string literal.
        const QJsonObject requisition {
            { QStringLiteral("jobId"), QStringLiteral("785338") },
            { QStringLiteral("contestNo"), QStringLiteral("193686") },
            { QStringLiteral("column"), QJsonArray {
                  QStringLiteral("Senior Control Systems Engineer"),
                  QStringLiteral("[\"United States-Ohio-Columbus\"]"),
                  QStringLiteral("Jun 15, 2026") } },
            { QStringLiteral("linkedColumn"), 0 },
            { QStringLiteral("locationsColumns"), QJsonArray { 1 } },
        };

        const JobPosting posting =
            TaleoBoardSource::postingFromRequisition(requisition, QStringLiteral("hdr/ex"));
        QCOMPARE(posting.positionTitle, QStringLiteral("Senior Control Systems Engineer"));
        QCOMPARE(posting.externalSourceId, QStringLiteral("193686"));
        QCOMPARE(posting.sourceUrl, QStringLiteral(
            "https://hdr.taleo.net/careersection/ex/jobdetail.ftl?job=193686&lang=en"));
        QCOMPARE(posting.locationText, QStringLiteral("Columbus, Ohio, United States"));
        QCOMPARE(posting.postedTimestamp.date(), QDate(2026, 6, 15));
        QCOMPARE(posting.companyName, QStringLiteral("HDR"));
    }

    void bothDateFormsAreRead()
    {
        QCOMPARE(TaleoBoardSource::postedDateFromColumnText(QStringLiteral("Oct 8, 2026, 6:16:00 AM")),
                 QDate(2026, 10, 8));
        QCOMPARE(TaleoBoardSource::postedDateFromColumnText(QStringLiteral("27/Oct/2026, 7:59:00 PM")),
                 QDate(2026, 10, 27));
        QVERIFY(!TaleoBoardSource::postedDateFromColumnText(QStringLiteral("Columbus")).isValid());
    }

    void theDescriptionIsReadFromTheJobPage()
    {
        // The names list, then the values list, as on the live page. The
        // description is "!*!" and URL-encoded HTML.
        const QString jobPageHtml = QStringLiteral(
            "descRequisition: {\n"
            "  _size: 1,\n"
            "  _hlid: ['reqlistitem.no','reqlistitem.title','reqlistitem.contestnumber',"
            "'reqlistitem.description','reqlistitem.qualification'],\n"
            "}\n"
            "api.fillList('requisitionDescriptionInterface', 'descRequisition', "
            "['785338','Senior Control Systems Engineer','193686',"
            "'!*!%3Cp%3EBuild control systems.%3C/p%3E',"
            "'!*!%3Cp%3EYou\\'ll need a degree.%3C/p%3E']);\n");

        const QString descriptionText = TaleoBoardSource::descriptionFromJobPage(jobPageHtml);
        QVERIFY2(descriptionText.contains(QStringLiteral("Build control systems.")),
                 qPrintable(descriptionText));
        QVERIFY2(descriptionText.contains(QStringLiteral("You'll need a degree.")),
                 qPrintable(descriptionText));
        QVERIFY(!descriptionText.contains(QLatin1Char('<')));
    }
};

QTEST_GUILESS_MAIN(TaleoBoardSourceTest)
#include "tst_TaleoBoardSource.moc"
