#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "../modelview/pipelines/JobPipelines.h"

class StagedDocumentRepository;
class StagingWorkbench;

// StagedJobListViewModel
//
// The left column of the Staging page: every job on the board, and how far its
// packet has got.
//
// Every job, including ones with no packet yet. A job crushed an hour ago and
// not opened is usually the reason the user came to this screen.
class StagedJobListViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int jobCount READ rowCountForProperty NOTIFY stagedJobsChanged)

public:
    enum StagedJobRole {
        JobApplicationIdRole = Qt::UserRole + 1,
        CompanyNameRole,
        PositionTitleRole,
        PipelineStageNameRole,
        FitScoreTextRole,
        PacketProgressTextRole,
        HasSomethingToSendRole
    };

    StagedJobListViewModel(JobPipelines &board,
                           StagedDocumentRepository &packetRepository,
                           StagingWorkbench &workbench,
                           QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountForProperty() const;

    // The campaign id for a row, so the packet view can be told which job to
    // show.
    Q_INVOKABLE qint64 jobApplicationIdAt(int rowIndex) const;

signals:
    void stagedJobsChanged();

private:
    void reload();

    struct StagedJobRow {
        TargetedJob targetedJob;
        int pieceCount = 0;
        int approvedCount = 0;
    };

    JobPipelines &jobPipelines;
    StagedDocumentRepository &stagedDocumentRepository;
    QList<StagedJobRow> loadedRows;
};
