#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "../model/WorkExperience.h"

class CareerHistoryRepository;
class ProDocsIntake;

// WorkExperienceListViewModel
//
// Serves the jobs Job Crush read out of the user's documents, and takes their
// corrections back down.
//
// Every row is editable on purpose. Parsing a resume is a heuristic and it
// WILL be wrong sometimes; the difference between a useful app and an
// infuriating one is whether being wrong costs the user one click or costs
// them the feature.
class WorkExperienceListViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int workExperienceCount READ rowCountForProperty NOTIFY workExperiencesChanged)

    // How many entries are still just Job Crush's reading. The page uses this
    // to ask for a look rather than assuming the job is done.
    Q_PROPERTY(int unconfirmedCount READ unconfirmedCount NOTIFY workExperiencesChanged)

public:
    enum WorkExperienceRole {
        EmployerNameRole = Qt::UserRole + 1,
        RoleTitleRole,
        StartDateTextRole,
        EndDateTextRole,
        SummaryTextRole,
        SourceLineTextRole,
        IsConfirmedByUserRole
    };

    WorkExperienceListViewModel(CareerHistoryRepository &careerRepository,
                                ProDocsIntake &intake,
                                QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountForProperty() const;
    int unconfirmedCount() const;

    // One setter per field rather than a generic one: the QML says what it
    // means, and a typo in a field name fails loudly instead of silently
    // writing nothing.
    Q_INVOKABLE void setEmployerNameAt(int rowIndex, const QString &employerName);
    Q_INVOKABLE void setRoleTitleAt(int rowIndex, const QString &roleTitle);
    Q_INVOKABLE void setStartDateTextAt(int rowIndex, const QString &startDateText);
    Q_INVOKABLE void setEndDateTextAt(int rowIndex, const QString &endDateText);
    Q_INVOKABLE void setSummaryTextAt(int rowIndex, const QString &summaryText);

    // "Yes, that's right." Confirming is what turns a reading into a fact.
    Q_INVOKABLE void setConfirmedAt(int rowIndex, bool isConfirmed);

    Q_INVOKABLE void addEmptyWorkExperience();
    Q_INVOKABLE void removeWorkExperienceAt(int rowIndex);

signals:
    void workExperiencesChanged();

private:
    void reload();
    bool saveRow(int rowIndex);

    // saveRow, plus the record that a person wrote this. See the .cpp.
    bool saveRowTheUserTypedInto(int rowIndex);

    CareerHistoryRepository &repository;
    QList<WorkExperience> loadedWorkExperiences;
};
