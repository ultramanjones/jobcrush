#include "WorkExperienceListViewModel.h"

#include "../model/CareerHistoryRepository.h"
#include "../modelview/prodocs/ProDocsIntake.h"

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
    int unconfirmed = 0;
    for (const WorkExperience &workExperience : loadedWorkExperiences) {
        if (!workExperience.isConfirmedByUser) {
            ++unconfirmed;
        }
    }
    return unconfirmed;
}

bool WorkExperienceListViewModel::saveRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()) {
        return false;
    }
    if (!repository.updateWorkExperience(loadedWorkExperiences.at(rowIndex))) {
        return false;
    }
    const QModelIndex changedIndex = index(rowIndex, 0);
    emit dataChanged(changedIndex, changedIndex);
    emit workExperiencesChanged();
    return true;
}

void WorkExperienceListViewModel::setEmployerNameAt(int rowIndex, const QString &employerName)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).employerName == employerName) {
        return;
    }
    loadedWorkExperiences[rowIndex].employerName = employerName;
    saveRow(rowIndex);
}

void WorkExperienceListViewModel::setRoleTitleAt(int rowIndex, const QString &roleTitle)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).roleTitle == roleTitle) {
        return;
    }
    loadedWorkExperiences[rowIndex].roleTitle = roleTitle;
    saveRow(rowIndex);
}

void WorkExperienceListViewModel::setStartDateTextAt(int rowIndex, const QString &startDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).startDateText == startDateText) {
        return;
    }
    loadedWorkExperiences[rowIndex].startDateText = startDateText;
    saveRow(rowIndex);
}

void WorkExperienceListViewModel::setEndDateTextAt(int rowIndex, const QString &endDateText)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).endDateText == endDateText) {
        return;
    }
    loadedWorkExperiences[rowIndex].endDateText = endDateText;
    saveRow(rowIndex);
}

void WorkExperienceListViewModel::setSummaryTextAt(int rowIndex, const QString &summaryText)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()
            || loadedWorkExperiences.at(rowIndex).summaryText == summaryText) {
        return;
    }
    loadedWorkExperiences[rowIndex].summaryText = summaryText;
    saveRow(rowIndex);
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
    if (repository.insertWorkExperience(workExperience)) {
        reload();
    }
}

void WorkExperienceListViewModel::removeWorkExperienceAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedWorkExperiences.count()) {
        return;
    }
    if (repository.removeWorkExperience(
            loadedWorkExperiences.at(rowIndex).workExperienceId)) {
        reload();
    }
}
