#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariantMap>

#include "../logic/jobscout/JobScout.h"

class JobPipelines;

// DiscoveredJobListViewModel
//
// This class gives the Discoveries page the list of jobs to show, one row
// per job. The page has tabs, and each tab shows a different list:
//  - Top Prospects: every found job inside the user's search area, with the
//    best match first.
//  - Manual Add: the jobs the user added by hand.
//  - Outside your area: the found jobs that the location filter hides,
//    because their location does not match any place the user named.
//  - One tab per job site: the jobs that site found inside the search area,
//    newest first.
//
// One object of this class serves every tab, because the page shows only one
// tab at a time. When the page sets activeTabSourceName, this class clears
// the old rows and asks for the rows of the new tab.
//
// A sweep is one run that asks every ticked job site for jobs. This class
// also reports on the sweep: whether it is running, its progress, and a
// summary when it ends. It also reports on adding one job by hand.
//
// This class only puts data into a form the page can show. JobScout, in the
// Logic layer, does the real work. JobScout finds jobs, saves them, removes
// duplicates and scores them.
class DiscoveredJobListViewModel : public QAbstractListModel {
    Q_OBJECT

    // --- Which tab is showing ---

    // The name of the tab that is showing. An empty string means Top
    // Prospects. If it equals manualAddTabSourceName, the tab is Manual Add.
    // If it equals outsideSearchAreaTabSourceName, the tab is Outside your
    // area. Any other value is the storage name of one job site, and the tab
    // shows that site's jobs.
    Q_PROPERTY(QString activeTabSourceName READ activeTabSourceName
                   WRITE setActiveTabSourceName NOTIFY activeTabSourceNameChanged)

    // The names of the two special tabs, Manual Add and Outside your area.
    // They are not job sites. The page reads these names from here, so each
    // name is written in one place only, in DiscoveredJobListViewModel.cpp.
    Q_PROPERTY(QString manualAddTabSourceName READ manualAddTabSourceName CONSTANT)
    Q_PROPERTY(QString outsideSearchAreaTabSourceName READ outsideSearchAreaTabSourceName
                   CONSTANT)

    Q_PROPERTY(bool showingManualAdd READ showingManualAdd
                   NOTIFY activeTabSourceNameChanged)
    Q_PROPERTY(bool showingOutsideSearchArea READ showingOutsideSearchArea
                   NOTIFY activeTabSourceNameChanged)

    // --- The rows ---

    Q_PROPERTY(int discoveredJobCount READ rowCountForProperty NOTIFY discoveredJobsChanged)

    // True while JobScout loads and scores the rows for the tab on a
    // background thread. It is also true at startup. While it is true, the
    // page says the rows are loading. Without it, the page would show the
    // "nothing found" card for a moment, because the list is empty until the
    // rows arrive.
    Q_PROPERTY(bool rowsAreLoading READ rowsAreLoading NOTIFY discoveredJobsChanged)

    // False until the user's search profile has enough in it to score jobs
    // against. While it is false, the page hides the match score on every
    // row, because every score would be 0%. If the list is also empty, the
    // page asks the user to fill in the search profile.
    Q_PROPERTY(bool canRankProspects READ canRankProspects NOTIFY discoveredJobsChanged)

    // jobCountOutsideSearchArea is how many found jobs the location filter
    // hides, because their location does not match a place the user named.
    // searchAreaIsNarrowed is true when the user has named at least one
    // place, so the filter is on. While the filter is on, the page always
    // shows the count. If the filter hid most jobs and the page did not say
    // so, the user would think the sweep was broken.
    Q_PROPERTY(int jobCountOutsideSearchArea READ jobCountOutsideSearchArea
                   NOTIFY discoveredJobsChanged)
    Q_PROPERTY(bool searchAreaIsNarrowed READ searchAreaIsNarrowed
                   NOTIFY discoveredJobsChanged)

    // --- The sweep ---

    // sweepIsRunning is true while at least one job site has not finished
    // answering. While it is true, the page shows sweepProgressText instead
    // of a spinner. sweepProgressText gives real job counts for each site.
    Q_PROPERTY(bool sweepIsRunning READ sweepIsRunning NOTIFY sweepProgressChanged)
    Q_PROPERTY(QString sweepProgressText READ sweepProgressText NOTIFY sweepProgressChanged)
    Q_PROPERTY(QString lastSweepSummaryText READ lastSweepSummaryText
                   NOTIFY sweepProgressChanged)

    // Each site that could not answer during the sweep, and the error it
    // gave. This text stays after the sweep ends. The summary text does not
    // replace it, so the user can still see what went wrong.
    Q_PROPERTY(QString lastSweepTroubleText READ lastSweepTroubleText
                   NOTIFY sweepProgressChanged)

    // A message from a site that is not a job count and not an error. Most
    // often the site asked Job Crush to wait before it asks again. The page
    // shows this message, but not as an error, because nothing is broken.
    Q_PROPERTY(QString lastSweepNoticeText READ lastSweepNoticeText
                   NOTIFY sweepProgressChanged)

    // --- Adding one job by hand ---

    // True while JobScout searches for the employer's own posting of a job
    // the user added by hand. While it is true, the page shows a sentence
    // that says what Job Crush is doing, instead of a spinner.
    Q_PROPERTY(bool leadIsBeingResolved READ leadIsBeingResolved NOTIFY leadStatusChanged)

    // One sentence that tells the user what happened to the job they added
    // by hand. It always ends with a next step the user can take.
    Q_PROPERTY(QString leadStatusText READ leadStatusText NOTIFY leadStatusChanged)

    // True when JobScout knows which job the user added by hand most
    // recently. The page shows the "Show me" button only when this is true.
    // That button scrolls to the job, so with no job it would do nothing.
    Q_PROPERTY(bool hasSomewhereToJumpTo READ hasSomewhereToJumpTo
                   NOTIFY leadStatusChanged)

    // --- Pasted text ---
    //
    // The user pastes text, such as a job alert email. Moonlight, the app's
    // AI brain, reads the text and fills in a form for one job.

    Q_PROPERTY(bool pastedTextIsBeingRead READ pastedTextIsBeingRead
                   NOTIFY pastedTextFormChanged)
    Q_PROPERTY(QString pastedTextStatusText READ pastedTextStatusText
                   NOTIFY pastedTextFormChanged)
    Q_PROPERTY(bool aBrainCanReadPastedText READ aBrainCanReadPastedText
                   NOTIFY pastedTextFormChanged)

    // The form Moonlight filled in, as a map with these keys: companyName,
    // positionTitle, sourceUrl, locationText, salaryText, postedText,
    // isRemoteRole, descriptionText and hasForm. hasForm is false when the
    // company, the job title and the description are all empty.
    Q_PROPERTY(QVariantMap filledForm READ filledForm NOTIFY pastedTextFormChanged)

public:
    enum DiscoveredJobRole {
        PositionTitleRole = Qt::UserRole + 1,
        CompanyNameRole,
        LocationTextRole,
        SalaryTextRole,
        SummaryLineRole,
        SourceDisplayNameRole,
        // The heading the page shows above each day's group of rows: "Today",
        // "Yesterday", or a date such as "August 21".
        PostedDayTextRole,
        MatchScoreRole,         // 0 to 100
        MatchReasonsTextRole,   // the reasons for the score, in plain words

        // "strong", "fair" or "weak". The page uses it to pick the color of
        // the score badge. ProspectScorer sets the scores where each word
        // starts, next to the scoring weights.
        MatchStrengthRole,

        IsRemoteRoleRole,

        // The name to show for the board or site Job Crush read this posting
        // from, such as "Ashby" or "Lever". Empty when Job Crush read the
        // posting from nowhere, and the job holds only what the user typed.
        PostingSourceDisplayNameRole,

        // True when the user added this job by hand.
        WasAddedByHandRole,

        // True when the user added this job by hand AND Job Crush could not
        // find the posting on any board or site. The row then holds only what
        // the user typed, so the user still needs to fill in the rest. The
        // page draws a 2-pixel outline in the theme's pending color around
        // these rows.
        IsManualAllTheWayRole,

        // The job's id in the database. Code that knows a job's id, but not
        // its row number, can use this to find the row.
        JobPostingIdRole,

        // PostingSeemsGoneRole is true when the company's own job board no
        // longer lists this job. Job Crush checks for this during sweeps that
        // read a company's whole board. PostingGoneTextRole is the sentence
        // the row shows, with the date the job went missing.
        PostingSeemsGoneRole,
        PostingGoneTextRole,

        // True when the posting asks for a security clearance. The page flags
        // these rows. It never hides them.
        AsksForClearanceRole,

        // True when this job is already on the user's board in Job Pipelines.
        // Then the row shows "on your board" instead of a CRUSH button.
        IsOnTheBoardRole
    };

    // Takes JobPipelines as well as JobScout. JobPipelines holds the user's
    // board. Each row has a CRUSH button, and that button puts the job on
    // the board through JobPipelines.
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
    bool rowsAreLoading() const;
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

    // Starts a sweep of every job site the user has ticked. If a sweep is
    // already running, JobScout ignores the call. Sweeps run only when the
    // user asks for one. Job Crush does not call other people's free job
    // feeds over and over in the background.
    Q_INVOKABLE void startSweep();

    // Opens the posting in row rowIndex in the user's web browser, on the
    // site that published it. Job Crush sends the user to the employer's
    // posting. It never shows the posting as if it were Job Crush's own.
    Q_INVOKABLE void openJobPostingInBrowser(int rowIndex) const;

    // The CRUSH button calls this. It puts the job in row rowIndex on the
    // user's board, in the Crushed column. It returns a sentence to show the
    // user. The sentence says what happened, whether the crush worked or not.
    // For example, it can say the job is already on the board.
    Q_INVOKABLE QString crushJobPostingAt(int rowIndex);

    // Returns the row number of the job the user added by hand most
    // recently, in the rows of the tab showing now. Returns -1 if that job is
    // not in these rows. After "Show me", the page scrolls to that row and
    // draws an outline around it for a while. The page decides how long.
    Q_INVOKABLE int rowOfMostRecentlyHandAddedJob() const;

    // Adds a job from a link the user pasted. The link can point to a site
    // Job Crush is allowed to read, or to one it is not. For example, Job
    // Crush does not read LinkedIn pages. But a LinkedIn link still names the
    // job well enough for Job Crush to search for it on other sites.
    Q_INVOKABLE void addJobFromLink(const QString &pastedLink);

    // Adds a job from a company name and a job title that the user typed,
    // for example from a job alert email.
    Q_INVOKABLE void addJobFromCompanyAndTitle(const QString &companyName,
                                               const QString &positionTitle);

    Q_INVOKABLE void readPastedText(const QString &pastedText);
    Q_INVOKABLE void clearFilledForm();

    // Adds a job from the form Moonlight filled in. It uses the value in each
    // box as the user left it, because the user may have changed it.
    // postedText is the date the job was posted. TypedDateReader reads it,
    // the same code that reads the applied date on a board card, so it
    // accepts many ways of writing a date.
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
    // Asks JobScout for the rows of the tab that is showing. JobScout loads
    // and scores them on a background thread. When they are ready,
    // whenRowsAreReady receives them.
    void requestRowsForTheActiveTab();

    // Receives rows from JobScout. Each request for rows has a number. This
    // shows the rows only if they answer the newest request. If a newer
    // request was made since, these rows are out of date, so this ignores
    // them. For example, the user may have switched tabs again.
    void whenRowsAreReady(int requestNumber, const DiscoveryRows &preparedRows);

    // Runs when the board in JobPipelines changes. Then any row may have
    // gone on or off the board. This tells the page to read
    // IsOnTheBoardRole again for every row.
    void refreshIsOnTheBoardForEveryRow();

    JobScout &discoveryJobScout;
    JobPipelines &jobPipelines;
    QString storedActiveTabSourceName;   // empty means Top Prospects
    QList<ScoredJobPosting> displayedJobPostings;
    int newestRowRequestNumber = 0;
    bool waitingForRows = true; // true at start: the constructor asks for rows
    int storedJobCountOutsideSearchArea = 0;
};
