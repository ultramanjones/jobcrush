#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "modelview/jobscout/AtsBoardDetector.h"
#include "modelview/jobscout/SmartRecruitersBoardSource.h"

namespace {

// Plain string literals, not raw ones: Qt 6.4's moc skips a file's class
// when the file holds raw string literals.
const char *listItemJson = "{\"id\":\"744000155992101\", \"name\":\"Purchasing Quality Engineer\", \"com"
    "pany\":{\"identifier\":\"BoschGroup\",\"name\":\"Bosch Group\"}, \"releasedD"
    "ate\":\"2026-10-16T13:13:01.485Z\", \"location\":{\"city\":\"Miskolc\",\"cou"
    "ntry\":\"hu\",\"remote\":false,\"hybrid\":false, \"fullLocation\":\"Miskolc,"
    " , Hungary\"}, \"ref\":\"https://api.smartrecruiters.com/v1/companies/BoschG"
    "roup/postings/744000155992101\"}";

const char *oneJobJson = "{\"id\":\"744000156080369\", \"name\":\"Senior Account Manager- Semiconducto"
    "rs\", \"company\":{\"name\":\"Bosch Group\",\"identifier\":\"BoschGroup\"}, "
    "\"location\":{\"city\":\"Sunnyvale\",\"region\":\"CA\",\"country\":\"us\",\""
    "remote\":true, \"fullLocation\":\"Sunnyvale, CA, United States\"}, \"release"
    "dDate\":\"2026-10-16T20:52:12.836Z\", \"postingUrl\":\"https://jobs.smartrec"
    "ruiters.com/BoschGroup/744000156080369-senior-account-manager-semiconductors"
    "\", \"jobAd\":{\"sections\":{ \"companyDescription\":{\"title\":\"Company De"
    "scription\",\"text\":\"<p><strong>We Are Bosch.</strong></p>\"}, \"jobDescri"
    "ption\":{\"title\":\"Job Description\",\"text\":\"<p>Manage component sales."
    "</p><ul><li>Quotations</li></ul>\"}, \"qualifications\":{\"title\":\"Qualifi"
    "cations\",\"text\":\"<p>5+ years in sales.</p>\"}, \"additionalInformation\""
    ":{\"title\":\"Additional Information\",\"text\":\"<p>Range $185,000 - $205,0"
    "00.</p>\"}}}, \"active\":true}";

} // namespace

// Tests for reading SmartRecruiters. The JSON below is cut down from real
// answers fetched on 2026-10-16 from BoschGroup's public postings.
class SmartRecruitersBoardSourceTest : public QObject {
    Q_OBJECT

private slots:
    void readsAListItem()
    {
        const JobPosting posting = SmartRecruitersBoardSource::postingFromJob(
            QJsonDocument::fromJson(QByteArray(listItemJson)).object(), QStringLiteral("BoschGroup"));

        QCOMPARE(posting.externalSourceId, QStringLiteral("744000155992101"));
        QCOMPARE(posting.positionTitle, QStringLiteral("Purchasing Quality Engineer"));
        QCOMPARE(posting.companyName, QStringLiteral("Bosch Group"));
        QCOMPARE(posting.locationText, QStringLiteral("Miskolc, Hungary"));
        QVERIFY(!posting.isRemoteRole);
        QCOMPARE(posting.postedTimestamp.date(), QDate(2026, 10, 16));
        QCOMPARE(posting.sourceUrl,
                 QStringLiteral("https://jobs.smartrecruiters.com/BoschGroup/744000155992101"));

        // The built link reads back as this exact job.
        const AtsBoardIdentity identity = AtsBoardDetector().identify(posting.sourceUrl);
        QCOMPARE(identity.boardName, AtsBoardName::SmartRecruiters);
        QCOMPARE(identity.tenant, QStringLiteral("BoschGroup"));
        QCOMPARE(identity.jobId, QStringLiteral("744000155992101"));
    }

    void readsTheOneJobAnswer()
    {
        const JobPosting posting = SmartRecruitersBoardSource::postingFromJob(
            QJsonDocument::fromJson(QByteArray(oneJobJson)).object(), QStringLiteral("BoschGroup"));

        QVERIFY(posting.sourceUrl.endsWith(QStringLiteral("-senior-account-manager-semiconductors")));
        QVERIFY(posting.isRemoteRole);
        QCOMPARE(posting.locationText, QStringLiteral("Sunnyvale, CA, United States (remote)"));
        QVERIFY(posting.fullDescriptionText.contains(QStringLiteral("Manage component sales.")));
        QVERIFY(posting.fullDescriptionText.contains(QStringLiteral("5+ years in sales.")));
        QVERIFY(!posting.fullDescriptionText.contains(QStringLiteral("<p>")));
        // The job itself comes before the company's own pitch.
        QVERIFY(posting.fullDescriptionText.indexOf(QStringLiteral("Manage component"))
                < posting.fullDescriptionText.indexOf(QStringLiteral("We Are Bosch")));
    }
};

QTEST_GUILESS_MAIN(SmartRecruitersBoardSourceTest)
#include "tst_SmartRecruitersBoardSource.moc"
