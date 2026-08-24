#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "../../model/StagedDocument.h"
#include "AiBrainTask.h"
#include "PostingRequirementReader.h"
#include "TaskInstructionWriter.h"
#include "TaskReplyReader.h"

class AiBrain;
class AiBrainReply;
class CareerHistoryRepository;
class JobApplicationRepository;
class JobPipelines;
class ProfessionalDocumentRepository;
class StagedDocumentRepository;

// StagingWorkbench
//
// Builds application packets. A ModelView class, and the only place that knows
// how a packet gets made.
//
// A job is crushed, a packet starts, and pieces get added over time. Some are
// written locally for free. Some are written by the AI when the user asks for
// them. Nothing here sends anything to anyone.
//
// What runs automatically:
//
//   On crush, with no AI needed: the checklist of what the employer asked for,
//   read from the posting text.
//
//   On crush, if an AI brain is connected: the plain-words summary of the
//   posting, and the fit score. Both are short and cheap, and both answer the
//   questions a user has right when they crush a job.
//
// What runs only when the user clicks a button: the cover letter, the tailored
// resume, and the follow-up. These are the expensive ones, and most crushed
// jobs never get applied to. Running them automatically would eat AI usage
// unnecessarily. (Decided 2026-08-23. Section 11 of the plan left it open.)
//
// One task runs at a time. The rest wait in a queue. Firing six requests at
// once gets a free tier rate-limited, and the user cannot read six answers at
// the same time anyway.
class StagingWorkbench : public QObject {
    Q_OBJECT
public:
    StagingWorkbench(AiBrain &brain,
                     JobPipelines &board,
                     StagedDocumentRepository &packetRepository,
                     JobApplicationRepository &applicationRepository,
                     CareerHistoryRepository &careerRepository,
                     ProfessionalDocumentRepository &documentRepository,
                     QObject *parent = nullptr);

    // Starts a packet for a job that was just crushed. Writes the checklist
    // now, and queues the summary and fit score if an AI brain is connected.
    // Safe to call twice. Nothing gets written twice.
    void startPacketFor(qint64 jobApplicationId);

    // Runs one task on demand. The buttons in Staging call this.
    // extraInstructionText is what the user typed in the box beside the
    // button. It wins over the built-in instructions if they conflict.
    void runTask(qint64 jobApplicationId,
                 AiBrainTaskKind taskKind,
                 const QString &extraInstructionText = QString());

    // Is a task in flight right now?
    bool isBusy() const;

    // The line the screen shows while a task runs, such as "Drafting the
    // letter". Empty when nothing is running.
    QString busyDescriptionText() const;

    // Text as it streams in for the piece being written right now. Watching
    // the text appear is the progress indicator. This app does not use
    // spinners.
    QString streamingText() const;

    // Which campaign the running task belongs to, so only that packet shows
    // the live text. 0 when nothing is running.
    qint64 busyJobApplicationId() const;

    // Whether an AI brain is connected. When false, the buttons are greyed out
    // and the page says why.
    bool aBrainIsAvailable() const;
    QString reasonNoBrainIsAvailable() const;

    // The user says they sent it. Moves the campaign to Applied, which stamps
    // the date.
    bool markPacketAsSent(qint64 jobApplicationId, QString &reasonText);

signals:
    // A packet gained, lost, or changed a piece.
    void packetChanged(qint64 jobApplicationId);

    // isBusy()/busyDescriptionText()/streamingText() changed.
    void busyStateChanged();

    // A task finished. The screen says so, instead of leaving the user to
    // notice a new card on their own.
    void taskFinished(qint64 jobApplicationId, const QString &whatWasDoneText);

    // A task failed. Two parts: what happened, and what to do about it. Every
    // failure message in this app ends with a next step.
    void taskFailed(qint64 jobApplicationId,
                    const QString &plainReason,
                    const QString &whatToDoNextText);

    // A task was skipped because the user had edited that piece themselves.
    // This is not an error, so it gets its own signal and softer wording.
    void taskRefusedBecauseUserEdited(qint64 jobApplicationId, const QString &pieceName);

private:
    struct QueuedTask {
        qint64 jobApplicationId = 0;
        AiBrainTaskKind taskKind = AiBrainTaskKind::ParsePosting;
        QString extraInstructionText;
    };

    void enqueueTask(const QueuedTask &task);
    void startNextTaskIfIdle();
    void finishActiveTask(const QString &completeReplyText);
    void abandonActiveTask(const QString &plainReason, const QString &whatToDoNextText);

    // Gathers everything the AI gets told about one job.
    bool buildBriefingFor(qint64 jobApplicationId, TaskBriefing &briefing) const;

    // The user's history as plain text, for the briefing.
    QString careerHistoryTextForBriefing() const;

    // The user's documents as plain text. The resume comes first and in full,
    // then anything else.
    QString documentTextForBriefing() const;

    // Writes a finished piece into the packet without overwriting user edits.
    void storeOutcome(qint64 jobApplicationId,
                      AiBrainTaskKind taskKind,
                      const TaskOutcome &outcome);

    static StagedDocumentKind stagedKindFor(AiBrainTaskKind taskKind);
    static QString titleFor(AiBrainTaskKind taskKind);

    AiBrain &aiBrain;
    JobPipelines &jobPipelines;
    StagedDocumentRepository &stagedDocumentRepository;
    JobApplicationRepository &jobApplicationRepository;
    CareerHistoryRepository &careerHistoryRepository;
    ProfessionalDocumentRepository &professionalDocumentRepository;

    TaskInstructionWriter instructionWriter;
    TaskReplyReader replyReader;
    PostingRequirementReader requirementReader;

    QList<QueuedTask> waitingTasks;
    QueuedTask activeTask;
    bool aTaskIsActive = false;
    QString accumulatedStreamingText;
    AiBrainReply *activeReply = nullptr; // owned via Qt parentage to this
};
