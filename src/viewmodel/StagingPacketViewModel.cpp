#include "StagingPacketViewModel.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QRegularExpression>
#include <QUrl>

#include "../model/StagedDocumentRepository.h"
#include "../modelview/AppPreferences.h"
#include "../modelview/exporting/ExportFolder.h"
#include "../modelview/exporting/PacketExporter.h"
#include "../modelview/pipelines/JobPipelines.h"
#include "../modelview/tasks/StagingWorkbench.h"

namespace {

QString displayNameForKind(StagedDocumentKind documentKind)
{
    switch (documentKind) {
    case StagedDocumentKind::CoverLetter:    return QStringLiteral("Cover letter");
    case StagedDocumentKind::TailoredResume: return QStringLiteral("Resume");
    case StagedDocumentKind::Checklist:      return QStringLiteral("Checklist");
    case StagedDocumentKind::PostingSummary: return QStringLiteral("The posting");
    case StagedDocumentKind::FitNote:        return QStringLiteral("Your odds");
    case StagedDocumentKind::FollowUpNote:   return QStringLiteral("Follow-up");
    case StagedDocumentKind::Other:          return QStringLiteral("Note");
    }
    return QStringLiteral("Note");
}

QString stageDisplayNameFor(PipelineStage pipelineStage)
{
    switch (pipelineStage) {
    case PipelineStage::Saved:     return QStringLiteral("On the board");
    case PipelineStage::Applied:   return QStringLiteral("Applied");
    case PipelineStage::Interview: return QStringLiteral("Interview");
    case PipelineStage::Offer:     return QStringLiteral("Offer");
    case PipelineStage::Closed:    return QStringLiteral("Closed");
    }
    return QStringLiteral("On the board");
}

} // namespace

StagingPacketViewModel::StagingPacketViewModel(StagingWorkbench &workbench,
                                               StagedDocumentRepository &packetRepository,
                                               JobPipelines &board,
                                               AppPreferences &preferences,
                                               const QString &applicationDataFolder,
                                               QObject *parent)
    : QAbstractListModel(parent)
    , stagingWorkbench(workbench)
    , stagedDocumentRepository(packetRepository)
    , jobPipelines(board)
    , appPreferences(preferences)
    , applicationDataFolderPath(applicationDataFolder)
{
    connect(&workbench, &StagingWorkbench::packetChanged, this,
            [this](qint64 jobApplicationId) {
                if (jobApplicationId == selectedJobApplicationId) {
                    reload();
                }
            });
    connect(&workbench, &StagingWorkbench::busyStateChanged, this,
            [this]() { emit busyStateChanged(); });
    connect(&workbench, &StagingWorkbench::taskFinished, this,
            [this](qint64, const QString &whatWasDone) {
                say(QStringLiteral("%1 is ready — read it before it goes anywhere.")
                        .arg(whatWasDone),
                    QString(), false);
            });
    connect(&workbench, &StagingWorkbench::taskFailed, this,
            [this](qint64, const QString &plainReason, const QString &whatToDoNext) {
                say(plainReason, whatToDoNext, true);
            });
    connect(&workbench, &StagingWorkbench::taskRefusedBecauseUserEdited, this,
            [this](qint64, const QString &pieceName) {
                say(QStringLiteral("Left your %1 alone — you've edited it, and your "
                                   "words aren't Moonlight's to overwrite.")
                        .arg(pieceName.toLower()),
                    QStringLiteral("Delete that piece if you'd like a fresh one written."),
                    false);
            });
    connect(&board, &JobPipelines::boardChanged, this,
            [this]() { emit selectedJobChanged(); });
    connect(&preferences, &AppPreferences::defaultDownloadFormatChanged, this,
            [this]() { emit downloadFormatChanged(); });
    connect(&preferences, &AppPreferences::exportFolderPathChanged, this,
            [this]() { emit exportFolderPathChanged(); });
}

// --- The list --------------------------------------------------------------

void StagingPacketViewModel::reload()
{
    beginResetModel();
    loadedPacket = selectedJobApplicationId > 0
        ? stagedDocumentRepository.loadPacketForApplication(selectedJobApplicationId)
        : QList<StagedDocument>();
    endResetModel();
}

int StagingPacketViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0;
    }
    return loadedPacket.count();
}

QVariant StagingPacketViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() >= loadedPacket.count()) {
        return QVariant();
    }
    const StagedDocument &piece = loadedPacket.at(modelIndex.row());
    switch (role) {
    case TitleTextRole:          return piece.titleText;
    case MarkdownTextRole:       return piece.markdownText;
    case KindNameRole:           return displayNameForKind(piece.documentKind);
    case WasWrittenByBrainRole:  return piece.wasWrittenByBrain;
    case WasEditedByUserRole:    return piece.wasEditedByUser;
    case IsApprovedByUserRole:   return piece.isApprovedByUser;
    case GoesToTheEmployerRole:  return belongsInTheSentPacket(piece.documentKind);
    }
    return QVariant();
}

QHash<int, QByteArray> StagingPacketViewModel::roleNames() const
{
    return {
        { TitleTextRole,         QByteArrayLiteral("titleText") },
        { MarkdownTextRole,      QByteArrayLiteral("markdownText") },
        { KindNameRole,          QByteArrayLiteral("kindName") },
        { WasWrittenByBrainRole, QByteArrayLiteral("wasWrittenByBrain") },
        { WasEditedByUserRole,   QByteArrayLiteral("wasEditedByUser") },
        { IsApprovedByUserRole,  QByteArrayLiteral("isApprovedByUser") },
        { GoesToTheEmployerRole, QByteArrayLiteral("goesToTheEmployer") },
    };
}

// --- Which packet ----------------------------------------------------------

void StagingPacketViewModel::showPacketFor(qint64 jobApplicationId)
{
    if (selectedJobApplicationId == jobApplicationId) {
        return;
    }
    selectedJobApplicationId = jobApplicationId;
    clearNotice();
    reload();
    emit selectedJobChanged();

    // Opening a packet that was never started starts it. The checklist is
    // free and needs no AI, so there is no reason to show an empty packet.
    if (loadedPacket.isEmpty() && jobApplicationId > 0) {
        stagingWorkbench.startPacketFor(jobApplicationId);
    }
}

bool StagingPacketViewModel::hasSelectedJob() const
{
    return selectedJobApplicationId > 0;
}

qint64 StagingPacketViewModel::currentJobApplicationId() const
{
    return selectedJobApplicationId;
}

QString StagingPacketViewModel::selectedCompanyName() const
{
    for (const TargetedJob &targetedJob : jobPipelines.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == selectedJobApplicationId) {
            return targetedJob.posting.companyName;
        }
    }
    return QString();
}

QString StagingPacketViewModel::selectedPositionTitle() const
{
    for (const TargetedJob &targetedJob : jobPipelines.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == selectedJobApplicationId) {
            return targetedJob.posting.positionTitle;
        }
    }
    return QString();
}

QString StagingPacketViewModel::selectedFitScoreText() const
{
    for (const TargetedJob &targetedJob : jobPipelines.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == selectedJobApplicationId) {
            return targetedJob.campaign.fitScorePercent >= 0
                ? QStringLiteral("%1% match").arg(targetedJob.campaign.fitScorePercent)
                : QString();
        }
    }
    return QString();
}

QString StagingPacketViewModel::selectedStageName() const
{
    for (const TargetedJob &targetedJob : jobPipelines.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == selectedJobApplicationId) {
            return stageDisplayNameFor(targetedJob.campaign.pipelineStage);
        }
    }
    return QString();
}

// --- What the brain is doing ----------------------------------------------

bool StagingPacketViewModel::isBusy() const
{
    return stagingWorkbench.isBusy()
        && stagingWorkbench.busyJobApplicationId() == selectedJobApplicationId;
}

QString StagingPacketViewModel::busyDescriptionText() const
{
    return isBusy() ? stagingWorkbench.busyDescriptionText() : QString();
}

QString StagingPacketViewModel::streamingText() const
{
    return isBusy() ? stagingWorkbench.streamingText() : QString();
}

bool StagingPacketViewModel::brainIsAvailable() const
{
    return stagingWorkbench.aBrainIsAvailable();
}

QString StagingPacketViewModel::reasonNoBrainIsAvailable() const
{
    return stagingWorkbench.reasonNoBrainIsAvailable();
}

// --- The buttons -----------------------------------------------------------

void StagingPacketViewModel::runTask(AiBrainTaskKind taskKind,
                                     const QString &extraInstructionText)
{
    if (selectedJobApplicationId <= 0) {
        return;
    }
    clearNotice();
    stagingWorkbench.runTask(selectedJobApplicationId, taskKind, extraInstructionText);
}

void StagingPacketViewModel::draftCoverLetter(const QString &extraInstructionText)
{
    runTask(AiBrainTaskKind::DraftCoverLetter, extraInstructionText);
}

void StagingPacketViewModel::tailorResume(const QString &extraInstructionText)
{
    runTask(AiBrainTaskKind::TailorResume, extraInstructionText);
}

void StagingPacketViewModel::workOutFollowUp(const QString &extraInstructionText)
{
    runTask(AiBrainTaskKind::SuggestFollowUp, extraInstructionText);
}

void StagingPacketViewModel::readThePostingAgain()
{
    runTask(AiBrainTaskKind::ParsePosting, QString());
}

void StagingPacketViewModel::scoreTheFit()
{
    runTask(AiBrainTaskKind::ScoreFit, QString());
}

// --- Editing ---------------------------------------------------------------

void StagingPacketViewModel::setMarkdownTextAt(int rowIndex, const QString &markdownText)
{
    if (rowIndex < 0 || rowIndex >= loadedPacket.count()) {
        return;
    }
    StagedDocument &piece = loadedPacket[rowIndex];
    if (piece.markdownText == markdownText) {
        return;
    }
    piece.markdownText = markdownText;

    // Once the user edits a piece, the AI never overwrites it. See
    // replaceGeneratedDocument.
    piece.wasEditedByUser = true;
    piece.lastEditedTimestamp = QDateTime::currentDateTime();

    if (!stagedDocumentRepository.updateStagedDocument(piece)) {
        say(QStringLiteral("Job Crush couldn't save that edit — %1")
                .arg(stagedDocumentRepository.lastErrorText()),
            QStringLiteral("Copy your text somewhere safe before you close this."), true);
        return;
    }
    emit dataChanged(index(rowIndex), index(rowIndex));
}

void StagingPacketViewModel::setApprovedAt(int rowIndex, bool isApproved)
{
    if (rowIndex < 0 || rowIndex >= loadedPacket.count()) {
        return;
    }
    StagedDocument &piece = loadedPacket[rowIndex];
    piece.isApprovedByUser = isApproved;
    if (!stagedDocumentRepository.updateStagedDocument(piece)) {
        say(QStringLiteral("Job Crush couldn't record that — %1")
                .arg(stagedDocumentRepository.lastErrorText()),
            QStringLiteral("Try the tick again."), true);
        return;
    }
    emit dataChanged(index(rowIndex), index(rowIndex));
}

void StagingPacketViewModel::removePieceAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedPacket.count()) {
        return;
    }
    const qint64 pieceId = loadedPacket.at(rowIndex).stagedDocumentId;
    if (!stagedDocumentRepository.removeStagedDocument(pieceId)) {
        say(QStringLiteral("Job Crush couldn't remove that — %1")
                .arg(stagedDocumentRepository.lastErrorText()),
            QStringLiteral("Try again in a moment."), true);
        return;
    }
    beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
    loadedPacket.removeAt(rowIndex);
    endRemoveRows();
}

void StagingPacketViewModel::addAPieceOfMyOwn()
{
    if (selectedJobApplicationId <= 0) {
        return;
    }
    StagedDocument piece;
    piece.jobApplicationId = selectedJobApplicationId;
    piece.documentKind = StagedDocumentKind::Other;
    piece.titleText = QStringLiteral("My own note");
    piece.wasEditedByUser = true;   // the user made it, so nothing overwrites it
    piece.createdTimestamp = QDateTime::currentDateTime();
    piece.lastEditedTimestamp = piece.createdTimestamp;

    if (!stagedDocumentRepository.insertStagedDocument(piece)) {
        say(QStringLiteral("Job Crush couldn't add that — %1")
                .arg(stagedDocumentRepository.lastErrorText()),
            QStringLiteral("Try again in a moment."), true);
        return;
    }
    reload();
}

// --- Out the door ----------------------------------------------------------

QString StagingPacketViewModel::downloadFormat() const
{
    return appPreferences.defaultDownloadFormat();
}

void StagingPacketViewModel::setDownloadFormat(const QString &downloadFormat)
{
    appPreferences.setDefaultDownloadFormat(downloadFormat);
}

QString StagingPacketViewModel::downloadFormatDisplayName() const
{
    return ExportFormat::displayNameFor(appPreferences.defaultDownloadFormat());
}

QString StagingPacketViewModel::folderExportsGoTo() const
{
    return ExportFolder::folderJobCrushWritesTo(appPreferences, applicationDataFolderPath);
}

QString StagingPacketViewModel::exportFolderPath() const
{
    return folderExportsGoTo();
}

QString StagingPacketViewModel::fileNameForCurrentPacket() const
{
    QString fileName = QStringLiteral("%1 - %2")
                           .arg(selectedCompanyName(), selectedPositionTitle())
                           .trimmed();
    if (fileName == QStringLiteral("-")) {
        fileName = QStringLiteral("Application packet");
    }

    // Windows rejects these characters in filenames.
    static const QRegularExpression charactersAFilesystemRefuses(
        QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1F]"));
    fileName.remove(charactersAFilesystemRefuses);
    fileName = fileName.simplified();

    return fileName.isEmpty() ? QStringLiteral("Application packet") : fileName;
}

void StagingPacketViewModel::exportPacketNow()
{
    if (selectedJobApplicationId <= 0) {
        return;
    }

    PacketExporter packetExporter;
    const PacketExporter::ExportOutcome outcome =
        packetExporter.exportPacket(loadedPacket, appPreferences.defaultDownloadFormat(),
                                    folderExportsGoTo(), fileNameForCurrentPacket());

    if (!outcome.succeeded) {
        say(outcome.reasonText, outcome.whatToDoNextText, true);
        return;
    }
    say(QStringLiteral("Written as %1.")
            .arg(QDir::toNativeSeparators(outcome.writtenFilePath)),
        QStringLiteral("Read it once before you attach it — it has your name on it."),
        false);
}

void StagingPacketViewModel::openExportFolder()
{
    const QString folderPath = folderExportsGoTo();
    QDir().mkpath(folderPath);
    QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath));
}

void StagingPacketViewModel::markAsSent()
{
    if (selectedJobApplicationId <= 0) {
        return;
    }
    QString reasonText;
    const bool wasMoved = stagingWorkbench.markPacketAsSent(selectedJobApplicationId,
                                                            reasonText);
    say(reasonText, QString(), !wasMoved);
    emit selectedJobChanged();
}

// --- The notice line -------------------------------------------------------

void StagingPacketViewModel::say(const QString &text, const QString &nextStepText,
                                 bool isAProblem)
{
    currentNoticeText = text;
    currentNoticeNextStepText = nextStepText;
    currentNoticeIsAProblem = isAProblem;
    emit noticeChanged();
}

QString StagingPacketViewModel::noticeText() const
{
    return currentNoticeText;
}

QString StagingPacketViewModel::noticeNextStepText() const
{
    return currentNoticeNextStepText;
}

bool StagingPacketViewModel::noticeIsAProblem() const
{
    return currentNoticeIsAProblem;
}

void StagingPacketViewModel::clearNotice()
{
    if (currentNoticeText.isEmpty() && currentNoticeNextStepText.isEmpty()) {
        return;
    }
    currentNoticeText.clear();
    currentNoticeNextStepText.clear();
    currentNoticeIsAProblem = false;
    emit noticeChanged();
}
