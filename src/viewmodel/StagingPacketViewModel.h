#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "../model/StagedDocument.h"
#include "../modelview/tasks/AiBrainTask.h"

class AppPreferences;
class JobPipelines;
class StagedDocumentRepository;
class StagingWorkbench;

// StagingPacketViewModel
//
// One application packet, formatted for the screen: the pieces in it, what the
// AI is doing right now, and the two end actions - export it, and mark it
// sent.
//
// Formatting only, like every viewmodel here. It makes no decisions. What runs
// automatically, what a packet contains, and what goes in an export are all
// decided below it in StagingWorkbench and PacketExporter.
class StagingPacketViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(bool hasSelectedJob READ hasSelectedJob NOTIFY selectedJobChanged)
    Q_PROPERTY(qint64 currentJobApplicationId READ currentJobApplicationId
                   NOTIFY selectedJobChanged)
    Q_PROPERTY(QString selectedCompanyName READ selectedCompanyName NOTIFY selectedJobChanged)
    Q_PROPERTY(QString selectedPositionTitle READ selectedPositionTitle NOTIFY selectedJobChanged)
    Q_PROPERTY(QString selectedFitScoreText READ selectedFitScoreText NOTIFY selectedJobChanged)
    Q_PROPERTY(QString selectedStageName READ selectedStageName NOTIFY selectedJobChanged)

    Q_PROPERTY(bool isBusy READ isBusy NOTIFY busyStateChanged)
    Q_PROPERTY(QString busyDescriptionText READ busyDescriptionText NOTIFY busyStateChanged)
    Q_PROPERTY(QString streamingText READ streamingText NOTIFY busyStateChanged)

    Q_PROPERTY(bool brainIsAvailable READ brainIsAvailable NOTIFY brainAvailabilityChanged)
    Q_PROPERTY(QString reasonNoBrainIsAvailable READ reasonNoBrainIsAvailable
                   NOTIFY brainAvailabilityChanged)

    Q_PROPERTY(QString downloadFormat READ downloadFormat WRITE setDownloadFormat
                   NOTIFY downloadFormatChanged)
    Q_PROPERTY(QString downloadFormatDisplayName READ downloadFormatDisplayName
                   NOTIFY downloadFormatChanged)
    Q_PROPERTY(QString exportFolderPath READ exportFolderPath NOTIFY exportFolderPathChanged)

    Q_PROPERTY(QString noticeText READ noticeText NOTIFY noticeChanged)
    Q_PROPERTY(QString noticeNextStepText READ noticeNextStepText NOTIFY noticeChanged)
    Q_PROPERTY(bool noticeIsAProblem READ noticeIsAProblem NOTIFY noticeChanged)

public:
    enum PacketPieceRole {
        TitleTextRole = Qt::UserRole + 1,
        MarkdownTextRole,
        KindNameRole,
        WasWrittenByBrainRole,
        WasEditedByUserRole,
        IsApprovedByUserRole,
        GoesToTheEmployerRole
    };

    StagingPacketViewModel(StagingWorkbench &workbench,
                           StagedDocumentRepository &packetRepository,
                           JobPipelines &board,
                           AppPreferences &preferences,
                           const QString &applicationDataFolderPath,
                           QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // --- Which packet is on screen ----------------------------------------
    Q_INVOKABLE void showPacketFor(qint64 jobApplicationId);
    bool hasSelectedJob() const;
    qint64 currentJobApplicationId() const;
    QString selectedCompanyName() const;
    QString selectedPositionTitle() const;
    QString selectedFitScoreText() const;
    QString selectedStageName() const;

    // --- What the brain is doing ------------------------------------------
    bool isBusy() const;
    QString busyDescriptionText() const;
    QString streamingText() const;
    bool brainIsAvailable() const;
    QString reasonNoBrainIsAvailable() const;

    // --- The buttons ------------------------------------------------------
    Q_INVOKABLE void draftCoverLetter(const QString &extraInstructionText);
    Q_INVOKABLE void tailorResume(const QString &extraInstructionText);
    Q_INVOKABLE void workOutFollowUp(const QString &extraInstructionText);
    Q_INVOKABLE void readThePostingAgain();
    Q_INVOKABLE void scoreTheFit();

    // --- Editing what is there --------------------------------------------
    Q_INVOKABLE void setMarkdownTextAt(int rowIndex, const QString &markdownText);
    Q_INVOKABLE void setApprovedAt(int rowIndex, bool isApproved);
    Q_INVOKABLE void removePieceAt(int rowIndex);
    Q_INVOKABLE void addAPieceOfMyOwn();

    // --- Out the door -----------------------------------------------------
    QString downloadFormat() const;
    void setDownloadFormat(const QString &downloadFormat);
    QString downloadFormatDisplayName() const;
    QString exportFolderPath() const;

    // Writes the packet as one file and reports where it was saved.
    Q_INVOKABLE void exportPacketNow();

    // Opens the export folder in the system file manager. A user who just
    // exported usually wants to attach the file.
    Q_INVOKABLE void openExportFolder();

    Q_INVOKABLE void markAsSent();

    // --- The status line shown to the user -------------------------------
    QString noticeText() const;
    QString noticeNextStepText() const;
    bool noticeIsAProblem() const;
    Q_INVOKABLE void clearNotice();

signals:
    void selectedJobChanged();
    void busyStateChanged();
    void brainAvailabilityChanged();
    void downloadFormatChanged();
    void exportFolderPathChanged();
    void noticeChanged();

private:
    void reload();
    void say(const QString &text, const QString &nextStepText, bool isAProblem);
    void runTask(AiBrainTaskKind taskKind, const QString &extraInstructionText);

    // The default export folder: Documents/Job Crush Packets. Under Documents
    // rather than the app data folder, because the file belongs to the user.
    QString folderExportsGoTo() const;

    // "Acme Robotics - Firmware Engineer", with characters the filesystem
    // rejects removed.
    QString fileNameForCurrentPacket() const;

    StagingWorkbench &stagingWorkbench;
    StagedDocumentRepository &stagedDocumentRepository;
    JobPipelines &jobPipelines;
    AppPreferences &appPreferences;
    const QString applicationDataFolderPath;

    qint64 selectedJobApplicationId = 0;
    QList<StagedDocument> loadedPacket;

    QString currentNoticeText;
    QString currentNoticeNextStepText;
    bool currentNoticeIsAProblem = false;
};
