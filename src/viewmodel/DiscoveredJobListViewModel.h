#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantMap>

#include "../logic/jobscout/JobScout.h"

class JobPipelines;

// DiscoveredJobListViewModel
//
// Serves the Discoveries page its rows. The rows are one of these:
//  - Top Prospects: every job found, ranked best match first,
//  - Manual Add: the jobs the user added by hand,
//  - Outside your area: the jobs the location filter holds back,
//  - one job site's finds.
//
// One instance serves every tab, because only one tab is on screen at a
// time. Setting activeTabSourceName rebuilds the rows for that tab.
//
// It also carries the state of the sweep that finds those jobs (running,
// progress, summary), and the state of adding one job by hand.
//
// This class translates and organizes only. Finding, saving, removing
// duplicates and scoring all happen below, in JobScout (Logic layer).
class DiscoveredJobListViewModel : public QAbstractListModel {
    Q_OBJECT

    // --- Which tab is showing ---

    // Empty string means Top Prospects. manualAddTabSourceName means Manual
    // Add, and outsideSearchAreaTabSourceName means Outside your area. Any
    // other value is a job site's storage name.
    Q_PROPERTY(QString activeTabSourceName READ activeTabSourceName
                   WRITE setActiveTabSourceName NOTIFY activeTabSourceNameChanged)

    // The reserved tab names. The page reads them from here, so each name is
    // spelled in one place only.
    Q_PROPERTY(QString manualAddTabSourceName READ manualAddTabSourceName CONSTANT)
    Q_PROPERTY(QString outsideSearchAreaTabSourceName READ outsideSearchAreaTabSourceName
                   CONSTANT)

    Q_PROPERTY(bool showingManualAdd READ showingManualAdd
                   NOTIFY activeTabSourceNameChanged)
    Q_PROPERTY(bool showingOutsideSearchArea READ showingOutsideSearchArea
                   NOTIFY activeTabSourceNameChanged)

    // --- The rows ---

    Q_PROPERTY(int discoveredJobCount READ rowCountForProperty NOTIFY discoveredJobsChanged)

    // False until the user has told Job Crush what they are looking for. The
    // page says so on Top Prospects, instead of showing an order that means
    // nothing.
    Q_PROPERTY(bool canRankProspects READ canRankProspects NOTIFY discoveredJobsChanged)

    // How many jobs the location filter is holding back, and whether it is
    // filtering at all. The page always shows the count. Without it, a filter
    // that hides most jobs would look like a broken sweep.
    Q_PROPERTY(int jobCountOutsideSearchArea READ jobCountOutsideSearchArea
                   NOTIFY discoveredJobsChanged)
    Q_PROPERTY(bool searchAreaIsNarrowed READ searchAreaIsNarrowed
                   NOTIFY discoveredJobsChanged)

    // --- The sweep ---

    // True while sites are still answering. While this is true, the page
    // shows the real numbers in sweepProgressText instead of a spinner.
    Q_PROPERTY(bool sweepIsRunning READ sweepIsRunning NOTIFY sweepProgressChanged)
    Q_PROPERTY(QString sweepProgressText READ sweepProgressText NOTIFY sweepProgressChanged)
    Q_PROPERTY(QString lastSweepSummaryText READ lastSweepSummaryText
                   NOTIFY sweepProgressChanged)

    // Any site that could not answer, and what it said. Kept after the sweep
    // ends, so the summary does not replace it.
    Q_PROPERTY(QString lastSweepTroubleText READ lastSweepTroubleText
                   NOTIFY sweepProgressChanged)

    // A message from a site that is neither a count nor a fault, most often
    // a site asking Job Crush to wait. Shown, but not as an error.
    Q_PROPERTY(QString lastSweepNoticeText READ lastSweepNoticeText
                   NOTIFY sweepProgressChanged)

    // --- Adding one job by hand ---

    // True while Job Crush is looking for one hand-added job. The page says
    // what Job Crush is doing instead of showing a spinner.
    Q_PROPERTY(bool leadIsBeingResolved READ leadIsBeingResolved NOTIFY leadStatusChanged)

    // What happened, in one sentence. It always ends with something the user
    // can do next.
    Q_PROPERTY(QString leadStatusText READ leadStatusText NOTIFY leadStatusChanged)

    // True when there is a hand-added job to jump to, so the "show me" button
    // only appears when it has somewhere to go.
    Q_PROPERTY(bool hasSomewhereToJumpTo READ hasSomewhereToJumpTo
                   NOTIFY leadStatusChanged)

    // --- Pasted text, read into a form by Moonlight ---

    Q_PROPERTY(bool pastedTextIsBeingRead READ pastedTextIsBeingRead
                   NOTIFY pastedTextFormChanged)
    Q_PROPERTY(QString pastedTextStatusText READ pastedTextStatusText
                   NOTIFY pastedTextFormChanged)
    Q_PROPERTY(bool aBrainCanReadPastedText READ aBrainCanReadPastedText
                   NOTIFY pastedTextFormChanged)

    // The filled form: companyName, positionTitle, sourceUrl, locationText,
    // salaryText, postedText, isRemoteRole, descriptionText, and hasForm.
    Q_PROPERTY(QVariantMap filledForm READ filledForm NOTIFY pastedTextFormChanged)

public:
    enum DiscoveredJobRole {
        PositionTitleRole = Qt::UserRole + 1,
        CompanyNameRole,
        LocationTextRole,
        SalaryTextRole,
        SummaryLineRole,
        SourceDisplayNameRole,
        PostedDayTextRole,      // the section header: "Today", "August 21"
        MatchScoreRole,         // 0 to 100
        MatchReasonsTextRole,   // why it scored that, in plain words

        // "strong", "fair" or "weak", for the color of the score badge. The
        // cutoffs live in ProspectScorer with the weights.
        MatchStrengthRole,

        IsRemoteRoleRole,

        // Which board the posting was read off, ready to show: "Ashby",
        // "Lever". Empty when it was not read off any board.
        PostingSourceDisplayNameRole,

        // True when the user added this job by hand.
        WasAddedByHandRole,

        // True when the user added it AND no board had it. These rows still
        // need the user to fill in the rest, and the page marks them.
        IsManualAllTheWayRole,

        // The database id, so a row can be found by something that only
        // knows which job it wants.
        JobPostingIdRole,

        // The company's own board no longer lists this job, as of the last
        // sweep that read their whole board.
        PostingSeemsGoneRole,
        PostingGoneTextRole,

        // The posting asks for a security clearance. Flagged, never hidden.
        AsksForClearanceRole,

        // The job is already on the board, so the row shows "on your board"
        // instead of a CRUSH button.
        IsOnTheBoardRole
    };

    // Takes the board as well as JobScout, because the CRUSH button on each
    // row puts that job on the board.
    DiscoveredJobListViewModel(JobScout &jobScout, JobPipelines &jobPipelines,
                               QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString activeTabSourceName() const;
    void setActiveTabSourceName(const QString &sourceStorageName);
    QString manualAddTabSourceName() const;
    QString outsideSearchAreaTabSourceName() const;
    bool showingManualAdd() const;
    bool showingOutsideSearchArea() const;

    int rowCountForProperty() const;
    bool canRankProspects() const;
    int jobCountOutsideSearchArea() const;
    bool searchAreaIsNarrowed() const;

    bool sweepIsRunning() const;
    QString sweepProgressText() const;
    QString lastSweepSummaryText() const;
    QString lastSweepTroubleText() const;
    QString lastSweepNoticeText() const;

    bool leadIsBeingResolved() const;
    QString leadStatusText() const;
    bool hasSomewhereToJumpTo() const;

    bool pastedTextIsBeingRead() const;
    QString pastedTextStatusText() const;
    bool aBrainCanReadPastedText() const;
    QVariantMap filledForm() const;

    // Starts a sweep of every ticked site. Sweeps only run when the user
    // asks. Job Crush does not keep calling other people's free job feeds in
    // the background.
    Q_INVOKABLE void startSweep();

    // Opens the posting on the site that published it. Job Crush links to
    // the employer's posting and never shows it as its own.
    Q_INVOKABLE void openJobPostingInBrowser(int rowIndex) const;

    // CRUSH: puts this job on the board, in the Crushed column. Returns the
    // sentence to show the user. It says what happened either way, including
    // "already on your board".
    Q_INVOKABLE QString crushJobPostingAt(int rowIndex);

    // Where the job the user added most recently is in the rows showing now,
    // or -1 when it is not among them. The page scrolls to that row and
    // marks it.
    Q_INVOKABLE int rowOfMostRecentlyHandAddedJob() const;

    // Paste a job link. This works for a link Job Crush can read, and for one
    // it cannot. A LinkedIn link still names the job well enough to look for
    // it somewhere else.
    Q_INVOKABLE void addJobFromLink(const QString &pastedLink);

    // Type the company and the job title, for example off an alert email.
    Q_INVOKABLE void addJobFromCompanyAndTitle(const QString &companyName,
                                               const QString &positionTitle);

    Q_INVOKABLE void readPastedText(const QString &pastedText);
    Q_INVOKABLE void clearFilledForm();

    // Adds the job from the form as the user left it. postedText is read the
    // same forgiving way as the applied date on a board card.
    Q_INVOKABLE void addJobFromFilledForm(const QString &companyName,
                                          const QString &positionTitle,
                                          const QString &sourceUrl,
                                          const QString &locationText,
                                          const QString &salaryText,
                                          const QString &postedText,
                                          bool isRemoteRole);

signals:
    void activeTabSourceNameChanged();
    void discoveredJobsChanged();
    void sweepProgressChanged();
    void leadStatusChanged();
    void pastedTextFormChanged();

private:
    void rebuildRowsFromJobScout();

    // The board changed, so "is it on the board" may have changed on any
    // row. Tells the page to read that role again for every row.
    void refreshIsOnTheBoardForEveryRow();

    JobScout &discoveryJobScout;
    JobPipelines &jobPipelines;
    QString storedActiveTabSourceName;   // empty means Top Prospects
    QList<ScoredJobPosting> displayedJobPostings;
};
