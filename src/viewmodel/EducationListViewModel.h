#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "../model/EducationRecord.h"

class CareerHistoryRepository;
class ProDocsIntake;

// EducationListViewModel
//
// Serves the schooling Job Crush read out of the user's documents, and takes
// their corrections back down. Same shape and same reasoning as
// WorkExperienceListViewModel — every row is editable, because a parse that
// cannot be corrected is a parse nobody can rely on.
class EducationListViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int educationCount READ rowCountForProperty NOTIFY educationRecordsChanged)
    Q_PROPERTY(int unconfirmedCount READ unconfirmedCount NOTIFY educationRecordsChanged)

public:
    enum EducationRole {
        SchoolNameRole = Qt::UserRole + 1,
        CredentialTextRole,
        FieldOfStudyTextRole,
        StartDateTextRole,
        EndDateTextRole,
        SourceLineTextRole,
        IsConfirmedByUserRole
    };

    EducationListViewModel(CareerHistoryRepository &careerRepository,
                           ProDocsIntake &intake,
                           QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountForProperty() const;
    int unconfirmedCount() const;

    Q_INVOKABLE void setSchoolNameAt(int rowIndex, const QString &schoolName);
    Q_INVOKABLE void setCredentialTextAt(int rowIndex, const QString &credentialText);
    Q_INVOKABLE void setFieldOfStudyTextAt(int rowIndex, const QString &fieldOfStudyText);
    Q_INVOKABLE void setStartDateTextAt(int rowIndex, const QString &startDateText);
    Q_INVOKABLE void setEndDateTextAt(int rowIndex, const QString &endDateText);
    Q_INVOKABLE void setConfirmedAt(int rowIndex, bool isConfirmed);

    Q_INVOKABLE void addEmptyEducationRecord();
    Q_INVOKABLE void removeEducationRecordAt(int rowIndex);

signals:
    void educationRecordsChanged();

private:
    void reload();
    bool saveRow(int rowIndex);

    CareerHistoryRepository &repository;
    QList<EducationRecord> loadedEducationRecords;
};
