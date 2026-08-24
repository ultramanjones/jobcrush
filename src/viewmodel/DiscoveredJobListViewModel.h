#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

#include "../modelview/jobscout/JobScout.h"

class JobPipelines;

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

    // What a site said that was neither a count nor a fault — a site asking to
    // be left alone for an hour, most often. Shown, but not painted as a
    // failure.
    Q_PROPERTY(QString lastSweepNoticeText READ lastSweepNoticeText
                   NOTIFY sweepProgressChanged)

    // False until the user has told Job Crush what they are looking for. Top
    // Prospects says so plainly rather than showing a meaningless order.
    Q_PROPERTY(bool canRankProspects READ canRankProspects NOTIFY discoveredJobsChanged)

    // How many jobs the location filter is holding back, and whether it is
    // filtering at all. The page states this out loud — a filter that quietly
    // eats jobs and never admits it looks exactly like a broken sweep, and
    // the user would go hunting for a bug that isn't there.
    Q_PROPERTY(int jobCountOutsideSearchArea READ jobCountOutsideSearchArea
                   NOTIFY discoveredJobsChanged)
    Q_PROPERTY(bool searchAreaIsNarrowed READ searchAreaIsNarrowed
                   NOTIFY discoveredJobsChanged)

    // Which side of the filter this list is showing. False is the normal
    // view; true is the "Outside your search area" look at what was held back.
    Q_PROPERTY(bool showingOutsideSearchArea READ showingOutsideSearchArea
                   WRITE setShowingOutsideSearchArea NOTIFY showingOutsideSearchAreaChanged)

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

    // Takes the board as well as the scout: a discovery's whole purpose is to
    // become something on the board, and the button that does it belongs on
    // the row where the job is, not on a screen the user has to go and find.
    DiscoveredJobListViewModel(JobScout &jobScout, JobPipelines &pipelines,
                               QObject *parent = nullptr);

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
    QString lastSweepNoticeText() const;
    bool canRankProspects() const;
    int jobCountOutsideSearchArea() const;
    bool searchAreaIsNarrowed() const;
    bool showingOutsideSearchArea() const;
    void setShowingOutsideSearchArea(bool showOutside);

    // The "scout now" action. Discovery is on demand by design — Job Crush
    // does not sit in the background hammering other people's free APIs.
    Q_INVOKABLE void startSweep();

    // Opens the posting on the site that published it. Job Crush never
    // reproduces a posting as if it were its own.
    Q_INVOKABLE void openJobPostingInBrowser(int rowIndex) const;

    // CRUSH. Puts this job on the board at Crushed. Returns the sentence to
    // show the user — it says what happened either way, including "already on
    // your board", which is information rather than a failure.
    Q_INVOKABLE QString crushJobPostingAt(int rowIndex);

    // Whether this row is already on the board, so the row can say so instead
    // of offering a button that will only decline.
    Q_INVOKABLE bool jobPostingAtIsOnTheBoard(int rowIndex) const;

    bool leadIsBeingResolved() const;
    QString leadStatusText() const;

    // Paste a job link. Works for a link Job Crush can read directly and for
    // one it cannot — a LinkedIn link still names the job well enough to go
    // looking for it elsewhere.
    Q_INVOKABLE void addJobFromLink(const QString &pastedLink);

    // Type the company and the job title, straight off an alert email.
    Q_INVOKABLE void addJobFromCompanyAndTitle(const QString &companyName,
                                               const QString &positionTitle);

    // --- Adding one job by hand ---
    //
    // The way around the sites Job Crush is not allowed to read: paste the
    // link, or type the company and the title off a LinkedIn alert, and Job
    // Crush goes and finds the same job on the employer's own board.

    // True while Job Crush is out looking for one pasted job. The page says
    // what it is doing rather than showing a spinner.
    Q_PROPERTY(bool leadIsBeingResolved READ leadIsBeingResolved NOTIFY leadStatusChanged)

    // What happened, in one sentence. Always ends with something to do next.
    Q_PROPERTY(QString leadStatusText READ leadStatusText NOTIFY leadStatusChanged)

    // Bumped whenever the board changes, so the per-row bindings above
    // re-evaluate. Same technique as the credential roster.
    Q_PROPERTY(int boardRevision READ boardRevision NOTIFY boardChanged)
    int boardRevision() const;

signals:
    void boardChanged();

    void activeTabSourceNameChanged();
    void showingOutsideSearchAreaChanged();
    void discoveredJobsChanged();
    void sweepProgressChanged();
    void leadStatusChanged();

private:
    void rebuildRowsFromJobScout();

    JobScout &discoveryJobScout;
    JobPipelines &board;
    int boardChangeCounter = 0;
    QString storedActiveTabSourceName;   // empty = Top Prospects
    bool storedShowingOutsideSearchArea = false;
    QList<ScoredJobPosting> displayedJobPostings;
};
