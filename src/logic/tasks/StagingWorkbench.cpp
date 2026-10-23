#include "StagingWorkbench.h"

#include <QDateTime>

#include "../../model/CareerHistoryRepository.h"
#include "../../model/JobApplicationRepository.h"
#include "../../model/ProfessionalDocumentRepository.h"
#include "../../model/StagedDocumentRepository.h"
#include "../aibrain/AiBrain.h"
#include "../aibrain/AiBrainProvider.h"
#include "../aibrain/AiBrainReply.h"
#include "../pipelines/JobPipelines.h"
#include "../prodocs/DocumentKind.h"

StagingWorkbench::StagingWorkbench(AiBrain &brain,
                                   JobPipelines &board,
                                   StagedDocumentRepository &packetRepository,
                                   JobApplicationRepository &applicationRepository,
                                   CareerHistoryRepository &careerRepository,
                                   ProfessionalDocumentRepository &documentRepository,
                                   QObject *parent)
    : QObject(parent)
    , aiBrain(brain)
    , jobPipelines(board)
    , stagedDocumentRepository(packetRepository)
    , jobApplicationRepository(applicationRepository)
    , careerHistoryRepository(careerRepository)
    , professionalDocumentRepository(documentRepository)
{
}

bool StagingWorkbench::isBusy() const
{
    return aTaskIsActive;
}

qint64 StagingWorkbench::busyJobApplicationId() const
{
    return aTaskIsActive ? activeTask.jobApplicationId : 0;
}

QString StagingWorkbench::streamingText() const
{
    return accumulatedStreamingText;
}

QString StagingWorkbench::busyDescriptionText() const
{
    if (!aTaskIsActive) {
        return QString();
    }
    return aiBrainTaskDisplayName(activeTask.taskKind);
}

bool StagingWorkbench::aBrainIsAvailable() const
{
    return aiBrain.isConfigured();
}

QString StagingWorkbench::reasonNoBrainIsAvailable() const
{
    if (aiBrain.isConfigured()) {
        return QString();
    }
    return QStringLiteral("No AI brain is connected yet. Add a key in Settings and "
                          "Moonlight can take a run at this.");
}

// --- Starting a packet ----------------------------------------------------

void StagingWorkbench::startPacketFor(qint64 jobApplicationId)
{
    TaskBriefing briefing;
    if (!buildBriefingFor(jobApplicationId, briefing)) {
        return;
    }

    // The free part always runs first, so a packet still has something in it
    // when no AI brain is connected.
    StagedDocument checklistDocument;
    checklistDocument.jobApplicationId = jobApplicationId;
    checklistDocument.documentKind = StagedDocumentKind::Checklist;
    checklistDocument.titleText = titleFor(AiBrainTaskKind::ParsePosting).isEmpty()
        ? QStringLiteral("Checklist")
        : QStringLiteral("What they asked for");
    checklistDocument.markdownText =
        requirementReader.checklistMarkdownFor(briefing.posting.fullDescriptionText);
    checklistDocument.wasWrittenByBrain = false;
    checklistDocument.createdTimestamp = QDateTime::currentDateTime();
    checklistDocument.lastEditedTimestamp = checklistDocument.createdTimestamp;

    bool refusedBecauseEdited = false;
    if (stagedDocumentRepository.replaceGeneratedDocument(checklistDocument,
                                                          refusedBecauseEdited)) {
        emit packetChanged(jobApplicationId);
    }

    if (!aiBrain.isConfigured()) {
        return; // running with no AI brain is supported, not an error
    }

    // The two cheap questions a user has right after crushing a job: what is
    // this job, and do I have a shot at it.
    enqueueTask({ jobApplicationId, AiBrainTaskKind::ParsePosting, QString() });
    enqueueTask({ jobApplicationId, AiBrainTaskKind::ScoreFit, QString() });
    startNextTaskIfIdle();
}

void StagingWorkbench::runTask(qint64 jobApplicationId,
                                AiBrainTaskKind taskKind,
                                const QString &extraInstructionText)
{
    if (!aiBrain.isConfigured()) {
        emit taskFailed(jobApplicationId,
                        QStringLiteral("There's no AI brain connected."),
                        QStringLiteral("Add a key in Settings — one key is all it takes."));
        return;
    }
    enqueueTask({ jobApplicationId, taskKind, extraInstructionText });
    startNextTaskIfIdle();
}

void StagingWorkbench::enqueueTask(const QueuedTask &task)
{
    // The same task twice in the queue means the user double-clicked.
    for (const QueuedTask &waitingTask : waitingTasks) {
        if (waitingTask.jobApplicationId == task.jobApplicationId
                && waitingTask.taskKind == task.taskKind) {
            return;
        }
    }
    if (aTaskIsActive && activeTask.jobApplicationId == task.jobApplicationId
            && activeTask.taskKind == task.taskKind) {
        return;
    }
    waitingTasks.append(task);
    emit busyStateChanged();
}

void StagingWorkbench::startNextTaskIfIdle()
{
    if (aTaskIsActive || waitingTasks.isEmpty()) {
        return;
    }

    activeTask = waitingTasks.takeFirst();

    TaskBriefing briefing;
    if (!buildBriefingFor(activeTask.jobApplicationId, briefing)) {
        emit taskFailed(activeTask.jobApplicationId,
                        QStringLiteral("Job Crush couldn't find that job any more."),
                        QStringLiteral("It may have come off the board. Crush it again "
                                       "from Discoveries and the packet starts over."));
        startNextTaskIfIdle();
        return;
    }
    briefing.extraInstructionText = activeTask.extraInstructionText;

    QList<AiBrainConversationMessage> conversation;
    AiBrainConversationMessage instruction;
    instruction.author = AiBrainConversationMessage::Author::Human;
    instruction.messageText = instructionWriter.instructionsFor(activeTask.taskKind, briefing);
    conversation.append(instruction);

    accumulatedStreamingText.clear();
    activeReply = aiBrain.streamConversation(conversation, this);
    if (activeReply == nullptr) {
        emit taskFailed(activeTask.jobApplicationId,
                        QStringLiteral("No AI brain is connected and active."),
                        QStringLiteral("Settings will show you which brains are ready "
                                       "and what each one still needs."));
        startNextTaskIfIdle();
        return;
    }

    aTaskIsActive = true;
    emit busyStateChanged();

    connect(activeReply, &AiBrainReply::textFragmentReceived, this,
            [this](const QString &textFragment) {
                accumulatedStreamingText += textFragment;
                emit busyStateChanged();
            });
    connect(activeReply, &AiBrainReply::finished, this,
            [this](const QString &completeReplyText) { finishActiveTask(completeReplyText); });
    connect(activeReply, &AiBrainReply::failed, this,
            [this](const QString &plainReason) {
                const QString whatToDoNext = activeReply != nullptr
                    ? activeReply->suggestedNextStep()
                    : QString();
                abandonActiveTask(plainReason, whatToDoNext);
            });
}

void StagingWorkbench::finishActiveTask(const QString &completeReplyText)
{
    const QueuedTask finishedTask = activeTask;

    if (activeReply != nullptr) {
        activeReply->deleteLater();
        activeReply = nullptr;
    }
    aTaskIsActive = false;
    accumulatedStreamingText.clear();
    emit busyStateChanged();

    const TaskOutcome outcome = replyReader.read(finishedTask.taskKind, completeReplyText);
    if (!outcome.succeeded) {
        emit taskFailed(finishedTask.jobApplicationId, outcome.reasonText,
                        outcome.whatToDoNextText);
        startNextTaskIfIdle();
        return;
    }

    storeOutcome(finishedTask.jobApplicationId, finishedTask.taskKind, outcome);
    startNextTaskIfIdle();
}

void StagingWorkbench::abandonActiveTask(const QString &plainReason,
                                          const QString &whatToDoNextText)
{
    const QueuedTask abandonedTask = activeTask;

    if (activeReply != nullptr) {
        activeReply->deleteLater();
        activeReply = nullptr;
    }
    aTaskIsActive = false;
    accumulatedStreamingText.clear();
    emit busyStateChanged();

    emit taskFailed(abandonedTask.jobApplicationId, plainReason,
                    whatToDoNextText.isEmpty()
                        ? QStringLiteral("Try it again in a moment. If it keeps failing, "
                                         "Settings will say what the brain is complaining about.")
                        : whatToDoNextText);

    // Drop the rest of the queue too. Sending three more requests to a vendor
    // that just refused one can get the key banned.
    if (!waitingTasks.isEmpty()) {
        waitingTasks.clear();
        emit busyStateChanged();
    }
}

// --- Writing the result down ----------------------------------------------

StagedDocumentKind StagingWorkbench::stagedKindFor(AiBrainTaskKind taskKind)
{
    switch (taskKind) {
    case AiBrainTaskKind::ParsePosting:     return StagedDocumentKind::PostingSummary;
    case AiBrainTaskKind::ScoreFit:         return StagedDocumentKind::FitNote;
    case AiBrainTaskKind::DraftCoverLetter: return StagedDocumentKind::CoverLetter;
    case AiBrainTaskKind::TailorResume:     return StagedDocumentKind::TailoredResume;
    case AiBrainTaskKind::SuggestFollowUp:  return StagedDocumentKind::FollowUpNote;
    }
    return StagedDocumentKind::Other;
}

QString StagingWorkbench::titleFor(AiBrainTaskKind taskKind)
{
    switch (taskKind) {
    case AiBrainTaskKind::ParsePosting:     return QStringLiteral("The posting, in plain words");
    case AiBrainTaskKind::ScoreFit:         return QStringLiteral("How you match");
    case AiBrainTaskKind::DraftCoverLetter: return QStringLiteral("Cover letter");
    case AiBrainTaskKind::TailorResume:     return QStringLiteral("Resume, aimed at this job");
    case AiBrainTaskKind::SuggestFollowUp:  return QStringLiteral("Following up");
    }
    return QStringLiteral("Draft");
}

void StagingWorkbench::storeOutcome(qint64 jobApplicationId,
                                     AiBrainTaskKind taskKind,
                                     const TaskOutcome &outcome)
{
    StagedDocument stagedDocument;
    stagedDocument.jobApplicationId = jobApplicationId;
    stagedDocument.documentKind = stagedKindFor(taskKind);
    stagedDocument.titleText = titleFor(taskKind);
    stagedDocument.markdownText = outcome.markdownText;
    stagedDocument.wasWrittenByBrain = true;
    stagedDocument.createdTimestamp = QDateTime::currentDateTime();
    stagedDocument.lastEditedTimestamp = stagedDocument.createdTimestamp;

    bool refusedBecauseEdited = false;
    if (!stagedDocumentRepository.replaceGeneratedDocument(stagedDocument,
                                                           refusedBecauseEdited)) {
        emit taskFailed(jobApplicationId,
                        QStringLiteral("Job Crush couldn't file that draft — %1")
                            .arg(stagedDocumentRepository.lastErrorText()),
                        QStringLiteral("The text isn't lost if you copy it out of the "
                                       "panel now. Then try the button again."));
        return;
    }

    if (refusedBecauseEdited) {
        emit taskRefusedBecauseUserEdited(jobApplicationId, titleFor(taskKind));
        return;
    }

    if (taskKind == AiBrainTaskKind::ScoreFit && outcome.fitScorePercent >= 0) {
        jobApplicationRepository.updateFitScorePercent(jobApplicationId,
                                                       outcome.fitScorePercent);
        jobPipelines.loadFromDatabase();
    }

    emit packetChanged(jobApplicationId);
    emit taskFinished(jobApplicationId, titleFor(taskKind));
}

// --- Gathering the briefing -----------------------------------------------

bool StagingWorkbench::buildBriefingFor(qint64 jobApplicationId, TaskBriefing &briefing) const
{
    bool jobWasFound = false;
    for (const TargetedJob &targetedJob : jobPipelines.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == jobApplicationId) {
            briefing.posting = targetedJob.posting;
            briefing.userNotesText = targetedJob.campaign.notesText;
            jobWasFound = true;
            break;
        }
    }
    if (!jobWasFound) {
        return false;
    }

    briefing.careerHistoryText = careerHistoryTextForBriefing();
    briefing.professionalDocumentsText = documentTextForBriefing();
    return true;
}

QString StagingWorkbench::careerHistoryTextForBriefing() const
{
    // The repositories are not const, but reading from them changes nothing.
    CareerHistoryRepository &repository =
        const_cast<CareerHistoryRepository &>(careerHistoryRepository);

    QString historyText;

    const QList<WorkExperience> everyJob = repository.loadAllWorkExperiences();
    if (!everyJob.isEmpty()) {
        historyText += QStringLiteral("Jobs they have held:\n");
        for (const WorkExperience &heldJob : everyJob) {
            historyText += QStringLiteral("- %1%2%3%4\n")
                .arg(heldJob.roleTitle.isEmpty() ? QString() : heldJob.roleTitle,
                     heldJob.employerName.isEmpty()
                         ? QString()
                         : QStringLiteral(" at %1").arg(heldJob.employerName),
                     heldJob.startDateText.isEmpty() && heldJob.endDateText.isEmpty()
                         ? QString()
                         : QStringLiteral(" (%1 - %2)")
                               .arg(heldJob.startDateText, heldJob.endDateText),
                     heldJob.summaryText.isEmpty()
                         ? QString()
                         : QStringLiteral(": %1").arg(heldJob.summaryText));
        }
    }

    const QList<EducationRecord> everySchool = repository.loadAllEducationRecords();
    if (!everySchool.isEmpty()) {
        historyText += QStringLiteral("\nSchooling:\n");
        for (const EducationRecord &schooling : everySchool) {
            historyText += QStringLiteral("- %1%2%3%4\n")
                .arg(schooling.schoolName,
                     schooling.credentialText.isEmpty()
                         ? QString()
                         : QStringLiteral(", %1").arg(schooling.credentialText),
                     schooling.fieldOfStudyText.isEmpty()
                         ? QString()
                         : QStringLiteral(" in %1").arg(schooling.fieldOfStudyText),
                     schooling.endDateText.isEmpty()
                         ? QString()
                         : QStringLiteral(" (%1)").arg(schooling.endDateText));
        }
    }

    return historyText;
}

QString StagingWorkbench::documentTextForBriefing() const
{
    ProfessionalDocumentRepository &repository =
        const_cast<ProfessionalDocumentRepository &>(professionalDocumentRepository);

    // The resume first, and in full. A letter written from a summary comes out
    // vague.
    QString documentText;
    for (const ProfessionalDocument &resume :
             repository.loadProfessionalDocumentsOfKind(DocumentKind::Resume)) {
        if (resume.extractedText.trimmed().isEmpty()) {
            continue;
        }
        documentText += QStringLiteral("--- %1 ---\n%2\n\n")
                            .arg(resume.displayName, resume.extractedText.trimmed());
    }

    if (documentText.isEmpty()) {
        // No document was classified as a resume. Send whatever text there is,
        // rather than telling the AI this person has no history.
        documentText = repository.allExtractedTextJoined();
    }
    return documentText;
}

// --- Out the door ---------------------------------------------------------

bool StagingWorkbench::markPacketAsSent(qint64 jobApplicationId, QString &reasonText)
{
    if (!jobPipelines.moveToStage(jobApplicationId, PipelineStage::Applied)) {
        reasonText = QStringLiteral("Job Crush couldn't move that card to Applied. "
                                    "Nothing was lost — try it again from the board.");
        return false;
    }
    reasonText = QStringLiteral("Marked as sent. The card moved to Applied and the date "
                                "is stamped on it.");
    emit packetChanged(jobApplicationId);
    return true;
}
