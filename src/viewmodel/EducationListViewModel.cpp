#include "EducationListViewModel.h"

#include <QDebug>

#include "../model/CareerHistoryRepository.h"
#include "../logic/prodocs/ProDocsIntake.h"

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
    int unconfirmedRowCount = 0;
    for (const EducationRecord &educationRecord : loadedEducationRecords) {
        if (!educationRecord.isConfirmedByUser) {
            ++unconfirmedRowCount;
        }
    }
    return unconfirmedRowCount;
}

bool EducationListViewModel::saveRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()) {
        return false;
    }
    if (!repository.updateEducationRecord(loadedEducationRecords.at(rowIndex))) {
        qWarning().noquote() << QStringLiteral(
            "EducationListViewModel::saveRow: could not save education record %1 — %2")
                .arg(loadedEducationRecords.at(rowIndex).educationRecordId)
                .arg(repository.lastErrorText());
        return false;
    }
    const QModelIndex changedIndex = index(rowIndex, 0);
    emit dataChanged(changedIndex, changedIndex);
    emit educationRecordsChanged();
    return true;
}

// Saving a value the USER typed, as opposed to one Job Crush read.
//
// The difference is not cosmetic. When the reader improves and re-reads every
// document, it is allowed to throw away and redo its own output — and it must
// not touch a word somebody wrote. This is where a row stops being a reading
// and becomes the user's.
bool EducationListViewModel::saveRowTheUserTypedInto(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()) {
        return false;
    }
    loadedEducationRecords[rowIndex].wasEditedByUser = true;
    return saveRow(rowIndex);
}

void EducationListViewModel::setSchoolNameAt(int rowIndex, const QString &schoolName)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).schoolName == schoolName) {
        return;
    }
    loadedEducationRecords[rowIndex].schoolName = schoolName;
    saveRowTheUserTypedInto(rowIndex);
}

void EducationListViewModel::setCredentialTextAt(int rowIndex, const QString &credentialText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).credentialText == credentialText) {
        return;
    }
    loadedEducationRecords[rowIndex].credentialText = credentialText;
    saveRowTheUserTypedInto(rowIndex);
}

void EducationListViewModel::setFieldOfStudyTextAt(int rowIndex, const QString &fieldOfStudyText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).fieldOfStudyText == fieldOfStudyText) {
        return;
    }
    loadedEducationRecords[rowIndex].fieldOfStudyText = fieldOfStudyText;
    saveRowTheUserTypedInto(rowIndex);
}

void EducationListViewModel::setStartDateTextAt(int rowIndex, const QString &startDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).startDateText == startDateText) {
        return;
    }
    loadedEducationRecords[rowIndex].startDateText = startDateText;
    saveRowTheUserTypedInto(rowIndex);
}

void EducationListViewModel::setEndDateTextAt(int rowIndex, const QString &endDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()
            || loadedEducationRecords.at(rowIndex).endDateText == endDateText) {
        return;
    }
    loadedEducationRecords[rowIndex].endDateText = endDateText;
    saveRowTheUserTypedInto(rowIndex);
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
    educationRecord.isConfirmedByUser = true;
    educationRecord.wasEditedByUser = true; // typed by hand, so it is theirs
    if (!repository.insertEducationRecord(educationRecord)) {
        qWarning().noquote() << QStringLiteral(
            "EducationListViewModel::addEmptyEducationRecord: could not add an empty "
            "education record — %1")
                .arg(repository.lastErrorText());
        return;
    }
    reload();
}

void EducationListViewModel::removeEducationRecordAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedEducationRecords.count()) {
        return;
    }
    const qint64 educationRecordIdToRemove =
        loadedEducationRecords.at(rowIndex).educationRecordId;
    if (!repository.removeEducationRecord(educationRecordIdToRemove)) {
        qWarning().noquote() << QStringLiteral(
            "EducationListViewModel::removeEducationRecordAt: could not remove education "
            "record %1 — %2")
                .arg(educationRecordIdToRemove).arg(repository.lastErrorText());
        return;
    }
    reload();
}
