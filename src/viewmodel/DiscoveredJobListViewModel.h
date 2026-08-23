#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "../modelview/jobscout/JobScout.h"

// DiscoveredJobListViewModel
//
// Serves the Discoveries page its rows: either one job site's finds, or the
// ranked Top Prospects across all of them. One instance serves every tab,
// because only one tab is on screen at a time — switching tabs sets
// activeTabSourceName and the rows are rebuilt. Five near-identical viewmodels
// would be five things to keep in step for no gain.
//
// It also carries the state of the sweep that produces those rows (running,
// progress, summary) — that is the same subject, not a second one.
//
// Translation and organization only: the fetching, storing, deduplicating and
// scoring all happen below, in JobScout (ModelView).
class DiscoveredJobListViewModel : public QAbstractListModel {
    Q_OBJECT

    // Which tab is showing. Empty string means Top Prospects — the ranked
    // list across every site — and any other value is a site's storage name.
    Q_PROPERTY(QString activeTabSourceName READ activeTabSourceName
                   WRITE setActiveTabSourceName NOTIFY activeTabSourceNameChanged)

    Q_PROPERTY(int discoveredJobCount READ rowCountForProperty NOTIFY discoveredJobsChanged)

    // True while sites are still answering. The page shows real numbers off
    // sweepProgressText while this is true — never a spinner.
    Q_PROPERTY(bool sweepIsRunning READ sweepIsRunning NOTIFY sweepProgressChanged)
    Q_PROPERTY(QString sweepProgressText READ sweepProgressText NOTIFY sweepProgressChanged)
    Q_PROPERTY(QString lastSweepSummaryText READ lastSweepSummaryText
                   NOTIFY sweepProgressChanged)

    // Any site that could not answer, and what it said. Outlives the sweep,
    // so a failure is never erased by the summary that follows it.
    Q_PROPERTY(QString lastSweepTroubleText READ lastSweepTroubleText
                   NOTIFY sweepProgressChanged)

    // False until the user has told Job Crush what they are looking for. Top
    // Prospects says so plainly rather than showing a meaningless order.
    Q_PROPERTY(bool canRankProspects READ canRankProspects NOTIFY discoveredJobsChanged)

public:
    enum DiscoveredJobRole {
        PositionTitleRole = Qt::UserRole + 1,
        CompanyNameRole,
        LocationTextRole,
        SalaryTextRole,
        SummaryLineRole,
        SourceDisplayNameRole,
        PostedDayTextRole,      // the section header: "Today", "Aug 21"
        MatchScoreRole,         // 0-100
        MatchReasonsTextRole,   // why it scored that, in plain words
        IsRemoteRoleRole
    };

    DiscoveredJobListViewModel(JobScout &jobScout, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString activeTabSourceName() const;
    void setActiveTabSourceName(const QString &sourceStorageName);

    int rowCountForProperty() const;
    bool sweepIsRunning() const;
    QString sweepProgressText() const;
    QString lastSweepSummaryText() const;
    QString lastSweepTroubleText() const;
    bool canRankProspects() const;

    // The "scout now" action. Discovery is on demand by design — Job Crush
    // does not sit in the background hammering other people's free APIs.
    Q_INVOKABLE void startSweep();

    // Opens the posting on the site that published it. Job Crush never
    // reproduces a posting as if it were its own.
    Q_INVOKABLE void openJobPostingInBrowser(int rowIndex) const;

signals:
    void activeTabSourceNameChanged();
    void discoveredJobsChanged();
    void sweepProgressChanged();

private:
    void rebuildRowsFromJobScout();

    JobScout &discoveryJobScout;
    QString storedActiveTabSourceName;   // empty = Top Prospects
    QList<ScoredJobPosting> displayedJobPostings;
};
