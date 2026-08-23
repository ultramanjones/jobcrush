#include "EducationListViewModel.h"

#include "../model/CareerHistoryRepository.h"
#include "../modelview/prodocs/ProDocsIntake.h"

EducationListViewModel::EducationListViewModel(
    CareerHistoryRepository &careerRepository, ProDocsIntake &intake, QObject *parent)
    : QAbstractListModel(parent)
    , repository(careerRepository)
{
    connect(&intake, &ProDocsIntake::careerHistoryChanged, this, [this]() { reload(); });
    reload();
}

void EducationListViewModel::reload()
{
    beginResetModel();
    loadedEducationRecords = repository.loadAllEducationRecords();
    endResetModel();
    emit educationRecordsChanged();
}

int EducationListViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0;
    }
    return loadedEducationRecords.count();
}

QVariant EducationListViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() >= loadedEducationRecords.count()) {
        return QVariant();
    }
    const EducationRecord &educationRecord = loadedEducationRecords.at(modelIndex.row());
    switch (role) {
    case SchoolNameRole:        return educationRecord.schoolName;
    case CredentialTextRole:    return educationRecord.credentialText;
    case FieldOfStudyTextRole:  return educationRecord.fieldOfStudyText;
    case StartDateTextRole:     return educationRecord.startDateText;
    case EndDateTextRole:       return educationRecord.endDateText;
    case SourceLineTextRole:    return educationRecord.sourceLineText;
    case IsConfirmedByUserRole: return educationRecord.isConfirmedByUser;
    }
    return QVariant();
}

QHash<int, QByteArray> EducationListViewModel::roleNames() const
{
    return {
        { SchoolNameRole,        QByteArrayLiteral("schoolName") },
        { CredentialTextRole,    QByteArrayLiteral("credentialText") },
        { FieldOfStudyTextRole,  QByteArrayLiteral("fieldOfStudyText") },
        { StartDateTextRole,     QByteArrayLiteral("startDateText") },
        { EndDateTextRole,       QByteArrayLiteral("endDateText") },
        { SourceLineTextRole,    QByteArrayLiteral("sourceLineText") },
        { IsConfirmedByUserRole, QByteArrayLiteral("isConfirmedByUser") },
    };
}

int EducationListViewModel::rowCountForProperty() const
{
    return loadedEducationRecords.count();
}

int EducationListViewModel::unconfirmedCount() const
{
    int unconfirmed = 0;
    for (const EducationRecord &educationRecord : loadedEducationRecords) {
        if (!educationRecord.isConfirmedByUser) {
            ++unconfirmed;
        }
    }
    return unconfirmed;
}

bool EducationListViewModel::saveRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()) {
        return false;
    }
    if (!repository.updateEducationRecord(loadedEducationRecords.at(rowIndex))) {
        return false;
    }
    const QModelIndex changedIndex = index(rowIndex, 0);
    emit dataChanged(changedIndex, changedIndex);
    emit educationRecordsChanged();
    return true;
}

void EducationListViewModel::setSchoolNameAt(int rowIndex, const QString &schoolName)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).schoolName == schoolName) {
        return;
    }
    loadedEducationRecords[rowIndex].schoolName = schoolName;
    saveRow(rowIndex);
}

void EducationListViewModel::setCredentialTextAt(int rowIndex, const QString &credentialText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).credentialText == credentialText) {
        return;
    }
    loadedEducationRecords[rowIndex].credentialText = credentialText;
    saveRow(rowIndex);
}

void EducationListViewModel::setFieldOfStudyTextAt(int rowIndex, const QString &fieldOfStudyText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).fieldOfStudyText == fieldOfStudyText) {
        return;
    }
    loadedEducationRecords[rowIndex].fieldOfStudyText = fieldOfStudyText;
    saveRow(rowIndex);
}

void EducationListViewModel::setStartDateTextAt(int rowIndex, const QString &startDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).startDateText == startDateText) {
        return;
    }
    loadedEducationRecords[rowIndex].startDateText = startDateText;
    saveRow(rowIndex);
}

void EducationListViewModel::setEndDateTextAt(int rowIndex, const QString &endDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).endDateText == endDateText) {
        return;
    }
    loadedEducationRecords[rowIndex].endDateText = endDateText;
    saveRow(rowIndex);
}

void EducationListViewModel::setConfirmedAt(int rowIndex, bool isConfirmed)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()) {
        return;
    }
    loadedEducationRecords[rowIndex].isConfirmedByUser = isConfirmed;
    saveRow(rowIndex);
}

void EducationListViewModel::addEmptyEducationRecord()
{
    EducationRecord educationRecord;
    educationRecord.isConfirmedByUser = true; // typed by hand, so it is theirs
    if (repository.insertEducationRecord(educationRecord)) {
        reload();
    }
}

void EducationListViewModel::removeEducationRecordAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()) {
        return;
    }
    if (repository.removeEducationRecord(
            loadedEducationRecords.at(rowIndex).educationRecordId)) {
        reload();
    }
}
