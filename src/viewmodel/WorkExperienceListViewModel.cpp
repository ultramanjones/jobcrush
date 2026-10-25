#include "WorkExperienceListViewModel.h"

#include <QDebug>

#include "../model/CareerHistoryRepository.h"
#include "../logic/prodocs/ProDocsIntake.h"

WorkExperienceListViewModel::WorkExperienceListViewModel(
    CareerHistoryRepository &careerRepository, ProDocsIntake &intake, QObject *parent)
    : QAbstractListModel(parent)
    , repository(careerRepository)
{
    connect(&intake, &ProDocsIntake::careerHistoryChanged, this, [this]() { reload(); });
    reload();
}

void WorkExperienceListViewModel::reload()
{
    beginResetModel();
    loadedWorkExperiences = repository.loadAllWorkExperiences();
    endResetModel();
    emit workExperiencesChanged();
}

int WorkExperienceListViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0;
    }
    return loadedWorkExperiences.count();
}

QVariant WorkExperienceListViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() >= loadedWorkExperiences.count()) {
        return QVariant();
    }
    const WorkExperience &workExperience = loadedWorkExperiences.at(modelIndex.row());
    switch (role) {
    case EmployerNameRole:      return workExperience.employerName;
    case RoleTitleRole:         return workExperience.roleTitle;
    case StartDateTextRole:     return workExperience.startDateText;
    case EndDateTextRole:       return workExperience.endDateText;
    case SummaryTextRole:       return workExperience.summaryText;
    case SourceLineTextRole:    return workExperience.sourceLineText;
    case IsConfirmedByUserRole: return workExperience.isConfirmedByUser;
    }
    return QVariant();
}

QHash<int, QByteArray> WorkExperienceListViewModel::roleNames() const
{
    return {
        { EmployerNameRole,      QByteArrayLiteral("employerName") },
        { RoleTitleRole,         QByteArrayLiteral("roleTitle") },
        { StartDateTextRole,     QByteArrayLiteral("startDateText") },
        { EndDateTextRole,       QByteArrayLiteral("endDateText") },
        { SummaryTextRole,       QByteArrayLiteral("summaryText") },
        { SourceLineTextRole,    QByteArrayLiteral("sourceLineText") },
        { IsConfirmedByUserRole, QByteArrayLiteral("isConfirmedByUser") },
    };
}

int WorkExperienceListViewModel::rowCountForProperty() const
{
    return loadedWorkExperiences.count();
}

int WorkExperienceListViewModel::unconfirmedCount() const
{
    int unconfirmedRowCount = 0;
    for (const WorkExperience &workExperience : loadedWorkExperiences) {
        if (!workExperience.isConfirmedByUser) {
            ++unconfirmedRowCount;
        }
    }
    return unconfirmedRowCount;
}

bool WorkExperienceListViewModel::saveRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()) {
        return false;
    }
    if (!repository.updateWorkExperience(loadedWorkExperiences.at(rowIndex))) {
        qWarning().noquote() << QStringLiteral(
            "WorkExperienceListViewModel::saveRow: could not save work experience %1 — %2")
                .arg(loadedWorkExperiences.at(rowIndex).workExperienceId)
                .arg(repository.lastErrorText());
        return false;
    }
    const QModelIndex changedIndex = index(rowIndex, 0);
    emit dataChanged(changedIndex, changedIndex);
    emit workExperiencesChanged();
    return true;
}

// Saving a value the USER typed, as opposed to one Job Crush read.
//
// The difference is not cosmetic. When the reader improves and re-reads every
// document, it is allowed to throw away and redo its own output — and it must
// not touch a word somebody wrote. This is where a row stops being a reading
// and becomes the user's.
bool WorkExperienceListViewModel::saveRowTheUserTypedInto(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()) {
        return false;
    }
    loadedWorkExperiences[rowIndex].wasEditedByUser = true;
    return saveRow(rowIndex);
}

void WorkExperienceListViewModel::setEmployerNameAt(int rowIndex, const QString &employerName)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).employerName == employerName) {
        return;
    }
    loadedWorkExperiences[rowIndex].employerName = employerName;
    saveRowTheUserTypedInto(rowIndex);
}

void WorkExperienceListViewModel::setRoleTitleAt(int rowIndex, const QString &roleTitle)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).roleTitle == roleTitle) {
        return;
    }
    loadedWorkExperiences[rowIndex].roleTitle = roleTitle;
    saveRowTheUserTypedInto(rowIndex);
}

void WorkExperienceListViewModel::setStartDateTextAt(int rowIndex, const QString &startDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).startDateText == startDateText) {
        return;
    }
    loadedWorkExperiences[rowIndex].startDateText = startDateText;
    saveRowTheUserTypedInto(rowIndex);
}

void WorkExperienceListViewModel::setEndDateTextAt(int rowIndex, const QString &endDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).endDateText == endDateText) {
        return;
    }
    loadedWorkExperiences[rowIndex].endDateText = endDateText;
    saveRowTheUserTypedInto(rowIndex);
}

void WorkExperienceListViewModel::setSummaryTextAt(int rowIndex, const QString &summaryText)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).summaryText == summaryText) {
        return;
    }
    loadedWorkExperiences[rowIndex].summaryText = summaryText;
    saveRowTheUserTypedInto(rowIndex);
}

void WorkExperienceListViewModel::setConfirmedAt(int rowIndex, bool isConfirmed)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()) {
        return;
    }
    loadedWorkExperiences[rowIndex].isConfirmedByUser = isConfirmed;
    saveRow(rowIndex);
}

void WorkExperienceListViewModel::addEmptyWorkExperience()
{
    WorkExperience workExperience;
    // Typed by hand, so it is the user's own from the first keystroke and
    // survives every re-read of their documents.
    workExperience.isConfirmedByUser = true;
    workExperience.wasEditedByUser = true;
    if (!repository.insertWorkExperience(workExperience)) {
        qWarning().noquote() << QStringLiteral(
            "WorkExperienceListViewModel::addEmptyWorkExperience: could not add an empty "
            "work experience — %1")
                .arg(repository.lastErrorText());
        return;
    }
    reload();
}

void WorkExperienceListViewModel::removeWorkExperienceAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()) {
        return;
    }
    const qint64 workExperienceIdToRemove = loadedWorkExperiences.at(rowIndex).workExperienceId;
    if (!repository.removeWorkExperience(workExperienceIdToRemove)) {
        qWarning().noquote() << QStringLiteral(
            "WorkExperienceListViewModel::removeWorkExperienceAt: could not remove work "
            "experience %1 — %2")
                .arg(workExperienceIdToRemove).arg(repository.lastErrorText());
        return;
    }
    reload();
}
