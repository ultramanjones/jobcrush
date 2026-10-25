#include <QtTest>

#include "logic/activejobs/JobQuestFile.h"

// Tests for JobQuestFile: reading and writing the JobQuest.txt file that an
// outside AI writes for each job.
class JobQuestFileTest : public QObject {
    Q_OBJECT

private slots:
    void readsEveryLine()
    {
        const QString fileText = QStringLiteral(
            "Quest created by: Claude\n"
            "Company: Acme Robotics\n"
            "Title: Senior Qt Developer\n"
            "Link: https://jobs.example.com/123\n"
            "Found on: FlexJobs\n"
            "Status: Applied\n"
            "Applied on: 10/14/2026\n"
            "Pay: $140k\n"
            "Where: Remote\n"
            "Notes: Recruiter is Jane, follow up Friday\n");
        const JobQuestFile::ReadResult result = JobQuestFile::readQuestText(fileText);
        QVERIFY(result.hasWhatTheBoardNeeds);
        QVERIFY(result.problemSentences.isEmpty());
        QCOMPARE(result.quest.questCreatedBy, QStringLiteral("Claude"));
        QCOMPARE(result.quest.companyName, QStringLiteral("Acme Robotics"));
        QCOMPARE(result.quest.positionTitle, QStringLiteral("Senior Qt Developer"));
        QCOMPARE(result.quest.postingLink, QStringLiteral("https://jobs.example.com/123"));
        QCOMPARE(result.quest.foundOn, QStringLiteral("FlexJobs"));
        QCOMPARE(result.quest.statusText, QStringLiteral("Applied"));
        QCOMPARE(result.quest.payText, QStringLiteral("$140k"));
        QCOMPARE(result.quest.whereText, QStringLiteral("Remote"));
        QCOMPARE(result.quest.notesText, QStringLiteral("Recruiter is Jane, follow up Friday"));
    }

    void acceptsOtherNamesAndAnyCase()
    {
        // The file's raw bytes, as a Windows editor would save them: a byte
        // order mark first, then Windows line endings. Kept as plain bytes on
        // purpose. Inside QStringLiteral, the \x escapes can come out
        // differently on the Windows compiler, and this test failed only there.
        const QByteArray fileBytes(
            "\xEF\xBB\xBF" "COMPANY: Acme\r\n"
            "job title: Qt Dev\r\n"
            "URL: https://example.com/job\r\n"
            "location: Pittsburgh, PA\r\n"
            "salary: 120k\r\n");
        const JobQuestFile::ReadResult result =
            JobQuestFile::readQuestText(QString::fromUtf8(fileBytes));
        QVERIFY(result.hasWhatTheBoardNeeds);
        QCOMPARE(result.quest.companyName, QStringLiteral("Acme"));
        QCOMPARE(result.quest.positionTitle, QStringLiteral("Qt Dev"));
        QCOMPARE(result.quest.postingLink, QStringLiteral("https://example.com/job"));
        QCOMPARE(result.quest.whereText, QStringLiteral("Pittsburgh, PA"));
        QCOMPARE(result.quest.payText, QStringLiteral("120k"));
    }

    void notesRunOverSeveralLines()
    {
        const JobQuestFile::ReadResult result = JobQuestFile::readQuestText(QStringLiteral(
            "Company: Acme\nTitle: Qt Dev\nNotes: First line\nsecond line\n\nthird line\n"));
        QCOMPARE(result.quest.notesText,
                 QStringLiteral("First line\nsecond line\nthird line"));
    }

    void missingTitleIsAProblem()
    {
        const JobQuestFile::ReadResult result =
            JobQuestFile::readQuestText(QStringLiteral("Company: Acme\n"));
        QVERIFY(!result.hasWhatTheBoardNeeds);
        QCOMPARE(result.problemSentences.count(), 1);
        QVERIFY(result.problemSentences.first().contains(QStringLiteral("Title")));
    }

    void unknownStatusAndBadDateAreProblems()
    {
        const JobQuestFile::ReadResult result = JobQuestFile::readQuestText(QStringLiteral(
            "Company: Acme\nTitle: Qt Dev\nStatus: Pending\nApplied on: whenever\n"));
        QVERIFY(result.hasWhatTheBoardNeeds);
        QCOMPARE(result.problemSentences.count(), 2);
    }

    void statusWordsAreReadAndWritten()
    {
        PipelineStage stage = PipelineStage::Saved;
        QVERIFY(JobQuestFile::readStatus(QStringLiteral("Rejected"), stage));
        QCOMPARE(stage, PipelineStage::Closed);
        QVERIFY(JobQuestFile::readStatus(QStringLiteral(" interviewing. "), stage));
        QCOMPARE(stage, PipelineStage::Interview);
        QVERIFY(JobQuestFile::readStatus(QStringLiteral("crushed"), stage));
        QCOMPARE(stage, PipelineStage::Saved);
        QVERIFY(!JobQuestFile::readStatus(QStringLiteral("maybe"), stage));
        QCOMPARE(JobQuestFile::statusWordFor(PipelineStage::Interview),
                 QStringLiteral("Interviewing"));
    }

    void appliedOnIsReadMonthFirst()
    {
        QDate dateRead;
        QVERIFY(JobQuestFile::readAppliedOn(QStringLiteral("10/4/2026"),
                                            QDate(2026, 10, 16), dateRead));
        QCOMPARE(dateRead, QDate(2026, 10, 4));
        QCOMPARE(JobQuestFile::appliedOnTextFor(QDate(2026, 10, 4)),
                 QStringLiteral("10/04/2026"));
    }

    // Only Status, Applied on and Notes change. Every other line, including
    // ones Job Crush does not know, stays as it was, with its line endings.
    // A missing Applied on line is added at the end.
    void boardChangesKeepEveryOtherLine()
    {
        const QString existingText = QStringLiteral(
            "Quest created by: ChatGPT\r\n"
            "Company: Acme\r\n"
            "Title: Qt Dev\r\n"
            "Status: Applied\r\n"
            "Recruiter phone: 555-0100\r\n"
            "Notes: old note\r\n"
            "more old note\r\n"
            "Pay: 120k\r\n");
        const QString newText = JobQuestFile::questTextWithBoardChanges(
            existingText, QStringLiteral("Interview"), QStringLiteral("10/14/2026"),
            QStringLiteral("new note"));
        QCOMPARE(newText, QStringLiteral(
            "Quest created by: ChatGPT\r\n"
            "Company: Acme\r\n"
            "Title: Qt Dev\r\n"
            "Status: Interview\r\n"
            "Recruiter phone: 555-0100\r\n"
            "Notes: new note\r\n"
            "Pay: 120k\r\n"
            "Applied on: 10/14/2026\r\n"));
    }

    // A notes line that starts like a field ("Status: ...") is written with
    // "- " in front, so reading the file back does not take it as a field.
    void notesThatLookLikeLinesSurviveARoundTrip()
    {
        JobQuest quest;
        quest.companyName = QStringLiteral("Acme");
        quest.positionTitle = QStringLiteral("Qt Dev");
        quest.notesText = QStringLiteral("Call back\nStatus: unclear, ask Jane");
        const JobQuestFile::ReadResult result =
            JobQuestFile::readQuestText(JobQuestFile::questTextFor(quest));
        QCOMPARE(result.quest.statusText, QString());
        QCOMPARE(result.quest.notesText,
                 QStringLiteral("Call back\n- Status: unclear, ask Jane"));
    }

    void linksAreNeverReadAsLineNames()
    {
        const JobQuestFile::ReadResult result = JobQuestFile::readQuestText(QStringLiteral(
            "Company: Acme\nTitle: Qt Dev\nNotes: see\nhttps://example.com/a:b\n"));
        QCOMPARE(result.quest.notesText, QStringLiteral("see\nhttps://example.com/a:b"));
    }
};

QTEST_GUILESS_MAIN(JobQuestFileTest)
#include "tst_JobQuestFile.moc"
