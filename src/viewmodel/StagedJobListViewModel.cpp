#include "StagedJobListViewModel.h"

#include "../model/StagedDocumentRepository.h"
#include "../modelview/tasks/StagingWorkbench.h"

namespace {

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

StagedJobListViewModel::StagedJobListViewModel(JobPipelines &board,
                                               StagedDocumentRepository &packetRepository,
                                               StagingWorkbench &workbench,
                                               QObject *parent)
    : QAbstractListModel(parent)
    , jobPipelines(board)
    , stagedDocumentRepository(packetRepository)
{
    connect(&board, &JobPipelines::boardChanged, this, [this]() { reload(); });
    connect(&workbench, &StagingWorkbench::packetChanged, this, [this](qint64) { reload(); });
    reload();
}

void StagedJobListViewModel::reload()
{
    beginResetModel();
    loadedRows.clear();
    for (const TargetedJob &targetedJob : jobPipelines.everyTargetedJob()) {
        StagedJobRow row;
        row.targetedJob = targetedJob;
        row.pieceCount = stagedDocumentRepository.countForApplication(
            targetedJob.campaign.jobApplicationId);
        row.approvedCount = stagedDocumentRepository.approvedCountForApplication(
            targetedJob.campaign.jobApplicationId);
        loadedRows.append(row);
    }
    endResetModel();
    emit stagedJobsChanged();
}

int StagedJobListViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0;
    }
    return loadedRows.count();
}

int StagedJobListViewModel::rowCountForProperty() const
{
    return loadedRows.count();
}

qint64 StagedJobListViewModel::jobApplicationIdAt(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= loadedRows.count()) {
        return 0;
    }
    return loadedRows.at(rowIndex).targetedJob.campaign.jobApplicationId;
}

QVariant StagedJobListViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() >= loadedRows.count()) {
        return QVariant();
    }
    const StagedJobRow &row = loadedRows.at(modelIndex.row());

    switch (role) {
    case JobApplicationIdRole:
        return QVariant::fromValue(row.targetedJob.campaign.jobApplicationId);
    case CompanyNameRole:
        return row.targetedJob.posting.companyName;
    case PositionTitleRole:
        return row.targetedJob.posting.positionTitle;
    case PipelineStageNameRole:
        return stageDisplayNameFor(row.targetedJob.campaign.pipelineStage);
    case FitScoreTextRole:
        // -1 means it has not been scored. Return an empty string, not "0%".
        // An unscored job must not look like a job that scored zero.
        return row.targetedJob.campaign.fitScorePercent >= 0
            ? QStringLiteral("%1% match").arg(row.targetedJob.campaign.fitScorePercent)
            : QString();
    case PacketProgressTextRole:
        if (row.pieceCount == 0) {
            return QStringLiteral("Nothing staged yet");
        }
        if (row.approvedCount == 0) {
            return row.pieceCount == 1
                ? QStringLiteral("1 draft, unread")
                : QStringLiteral("%1 drafts, none checked off").arg(row.pieceCount);
        }
        return QStringLiteral("%1 of %2 checked off")
            .arg(row.approvedCount).arg(row.pieceCount);
    case HasSomethingToSendRole:
        return row.pieceCount > 0;
    }
    return QVariant();
}

QHash<int, QByteArray> StagedJobListViewModel::roleNames() const
{
    return {
        { JobApplicationIdRole,   QByteArrayLiteral("jobApplicationId") },
        { CompanyNameRole,        QByteArrayLiteral("companyName") },
        { PositionTitleRole,      QByteArrayLiteral("positionTitle") },
        { PipelineStageNameRole,  QByteArrayLiteral("pipelineStageName") },
        { FitScoreTextRole,       QByteArrayLiteral("fitScoreText") },
        { PacketProgressTextRole, QByteArrayLiteral("packetProgressText") },
        { HasSomethingToSendRole, QByteArrayLiteral("hasSomethingToSend") },
    };
}
