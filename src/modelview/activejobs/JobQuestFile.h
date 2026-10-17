#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

#include "../../model/PipelineStage.h"

// JobQuest
//
// What one JobQuest.txt file says about one job. Plain data.
//
// Every field is the text exactly as the file wrote it, trimmed. An empty
// field means the file did not say. Reading the text into dates and stages is
// done separately, so a value Job Crush cannot understand is still kept and
// can be shown back to the user.
struct JobQuest {
    QString questCreatedBy;   // who made this job: "Job Crush", "ChatGPT", "Claude"
    QString companyName;      // required
    QString positionTitle;    // required
    QString postingLink;
    QString foundOn;          // the site it was found on: "FlexJobs"
    QString statusText;       // one of the board's columns: "Applied"
    QString appliedOnText;    // the day the application was sent
    QString payText;
    QString whereText;        // a place, or "Remote"
    QString notesText;        // may run over several lines
};

// JobQuestFile
//
// Reads and writes JobQuest.txt, the file that lets any outside AI keep Job
// Crush up to date.
//
// The file is plain lines of "Name: value", one per line:
//
//     Quest created by: Claude
//     Company: Acme Robotics
//     Title: Senior Qt Developer
//     Status: Applied
//     Applied on: 10/14/2026
//
// Only Company and Title are required.
//
// The reader is forgiving on purpose. Outside AIs will write half-finished or
// slightly wrong files. A line name in any case is accepted, and so are a few
// common other names for the same line ("Job title", "URL", "Location"). A
// line with no name carries on the line above it, so notes can run over
// several lines. A line Job Crush does not know is kept when the file is
// written back. Every problem is reported in a plain sentence; nothing is
// skipped without saying so.
namespace JobQuestFile {

// The file's name inside each job folder.
inline const QString FileName = QStringLiteral("JobQuest.txt");

struct ReadResult {
    JobQuest quest;

    // One plain sentence per problem, for the user to read. Empty when the
    // file is fine.
    QStringList problemSentences;

    // False when Job Crush cannot put this job on the board at all: Company
    // or Title is missing.
    bool hasWhatTheBoardNeeds = false;
};

// Reads the text of a JobQuest.txt file.
ReadResult readQuestText(const QString &fileText);

// Reads statusText as a board column. Accepts the five column names and a few
// common other words ("Rejected" means Closed, "Interviewing" means
// Interview). Returns false when the word means nothing to Job Crush.
bool readStatus(const QString &statusText, PipelineStage &stageRead);

// The word a file uses for a stage. The same words as the board's column
// headings: Crushed, Applied, Interviewing, Offer, Closed.
QString statusWordFor(PipelineStage pipelineStage);

// Reads appliedOnText as a date. 10/14/2026 is read month first, the way the
// instructions given to outside AIs write it. today is passed in so the
// answer is the same every time it is tested.
bool readAppliedOn(const QString &appliedOnText, const QDate &today, QDate &dateRead);

// How a date is written into the file: 10/14/2026.
QString appliedOnTextFor(const QDate &appliedDate);

// A whole new file for a job.
QString questTextFor(const JobQuest &quest);

// Changes only the Status, Applied on and Notes lines of an existing file and
// leaves every other line exactly as it was, including lines Job Crush does
// not know. A line that is missing is added at the end. existingText may be
// empty, in which case this is the same as questTextFor.
QString questTextWithBoardChanges(const QString &existingText,
                                  const QString &statusText,
                                  const QString &appliedOnText,
                                  const QString &notesText);

} // namespace JobQuestFile
