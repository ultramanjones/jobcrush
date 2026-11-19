#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>

#include "../logic/pipelines/BoardColumnOrder.h"

class ActiveJobsFolder;
class AppPreferences;
class JobScout;
class JobPipelines;
class PostingStillListedChecker;

// JobPipelineBoardViewModel
//
// Serves the board to QML: six columns of cards, and the handful of verbs
// that move them.
//
// Deliberately NOT a QAbstractListModel. Every other list in Job Crush is one,
// because every other list is one list. A board is six lists. So each column
// is handed its own cards, instead of one model with a filter role that the
// view would have to sort out. Columns are small, even in a long job search,
// so rebuilding a column's list on every change is cheap and always correct.
//
// boardRevision works the same way as the credential roster's counter. QML
// bindings that call cardsInStage() also read the revision, so they
// re-evaluate the moment anything underneath changes.
class JobPipelineBoardViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int boardRevision READ boardRevision NOTIFY boardChanged)
    Q_PROPERTY(int totalCardCount READ totalCardCount NOTIFY boardChanged)

    // What just happened, in one sentence, for the strip at the top of the
    // page. Empty when there is nothing to say.
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY lastActionChanged)

    // The "are these still open?" check. True while employers are answering,
    // and the sentence says what is happening in real numbers.
    Q_PROPERTY(bool stillListedCheckIsRunning READ stillListedCheckIsRunning
                   NOTIFY stillListedCheckChanged)
    Q_PROPERTY(QString stillListedCheckText READ stillListedCheckText
                   NOTIFY stillListedCheckChanged)

public:
    JobPipelineBoardViewModel(JobPipelines &pipelines,
                              PostingStillListedChecker &stillListedChecker,
                              ActiveJobsFolder &activeJobsFolder,
                              JobScout &jobScout,
                              AppPreferences &appPreferences,
                              QObject *parent = nullptr);

    int boardRevision() const;
    int totalCardCount() const;
    QString lastActionText() const;
    bool stillListedCheckIsRunning() const;
    QString stillListedCheckText() const;

    // The stages, in board order, as storage names: "saved", "applied", … The
    // view asks rather than hardcoding the list, so adding a stage one day
    // means changing PipelineStage and nothing else.
    Q_INVOKABLE QStringList stageNamesInBoardOrder() const;

    // The heading a column shows for a stage: "Saved", "Interviewing"…
    Q_INVOKABLE QString displayLabelForStage(const QString &stageName) const;

    // What a column is FOR, in one plain line. Shown under the heading while
    // the column is empty, so the user knows what belongs there.
    Q_INVOKABLE QString explanationForStage(const QString &stageName) const;

    // The line shown when the mouse rests on a column's heading: exactly when
    // a card belongs in that column.
    Q_INVOKABLE QString hoverLineForStage(const QString &stageName) const;

    // True for a column that starts folded to a thin strip at the right edge
    // of the board: Escaped. The user clicks the strip to open it.
    Q_INVOKABLE bool stageStartsFolded(const QString &stageName) const;

    // The cards in one column, in the order the user picked for that column
    // with its sort buttons. Each is a plain object the
    // QML delegate reads by name: jobApplicationId, positionTitle,
    // companyName, locationText, salaryText, sourceName, sourceUrl,
    // isRemoteRole, stageName, notesText, targetedText, appliedText,
    // appliedDateEditText, appliedDateIsEditable, postingSeemsGone,
    // postingGoneText, userSaysStillOpen, userSaysStillOpenText,
    // canMoveToEscaped, postingRoutes (a list of { boardDisplayName,
    // postingUrl }), and jobFolderPath.
    Q_INVOKABLE QVariantList cardsInStage(const QString &stageName) const;

    Q_INVOKABLE int countInStage(const QString &stageName) const;

    // --- Sorting a column -------------------------------------------------
    //
    // Each column has three sort buttons: date, title and company. The names
    // the view passes in are "date", "title" and "company".

    // The three sort button names, left to right.
    Q_INVOKABLE QStringList sortChoiceNames() const;

    // True for the one button whose sort the column is using now.
    Q_INVOKABLE bool sortChoiceIsInUse(const QString &stageName,
                                       const QString &sortChoiceName) const;

    // The words on a sort button. A button not in use says what it sorts by:
    // "Date added", "Title", "Company". The Applied column's date button says
    // "Date applied". The button in use says which way the column is sorted:
    // "Newest first", "Title A-Z", "Company Z-A".
    Q_INVOKABLE QString sortButtonText(const QString &stageName,
                                       const QString &sortChoiceName) const;

    // The line shown when the mouse rests on a sort button.
    Q_INVOKABLE QString sortButtonHoverText(const QString &stageName,
                                            const QString &sortChoiceName) const;

    // The user clicked a sort button. If it is the button in use, then the
    // order flips. If not, then the column sorts by that button. The choice
    // is saved, so the column opens the same way next time.
    Q_INVOKABLE void chooseSortFor(const QString &stageName, const QString &sortChoiceName);

    // --- The verbs --------------------------------------------------------

    // Drops a card into a column. Called by the DropArea under each column.
    Q_INVOKABLE void moveCardToStage(qint64 jobApplicationId, const QString &stageName);

    Q_INVOKABLE void setNotesFor(qint64 jobApplicationId, const QString &notesText);

    // Sets the applied date from what the user typed on the open card:
    // "10/8", "8 Oct", "yesterday", "3 days ago". Empty text clears the date.
    // The sentence at the top of the page says what happened either way.
    // Returns false when the date was refused and nothing changed.
    Q_INVOKABLE bool setAppliedDateFor(qint64 jobApplicationId, const QString &typedDateText);

    Q_INVOKABLE void removeCardFromBoard(qint64 jobApplicationId);

    // Opens the posting where it was published. The board links out; it never
    // pretends to host somebody else's listing.
    Q_INVOKABLE void openPostingInBrowser(qint64 jobApplicationId) const;

    // Opens one particular route. The card lists every way of reaching a job
    // and the user chooses; this opens the one they chose.
    Q_INVOKABLE void openPostingRouteInBrowser(const QString &postingUrl) const;

    // Opens the card's job folder, where its JobQuest.txt, resume and cover
    // letter are kept.
    Q_INVOKABLE void openJobFolder(qint64 jobApplicationId) const;

    // Looks for the same job on the employer's own board and adds it as
    // another place to apply. The sentence at the top of the page says it
    // is looking; a new link on the card says it found one.
    Q_INVOKABLE void lookForOtherPlacesToApply(qint64 jobApplicationId);

    Q_INVOKABLE void clearLastAction();

    // Asks each employer's board whether the jobs on this board are still
    // listed. Cards whose job is gone get a note. Nothing is removed.
    Q_INVOKABLE void checkWhichJobsAreStillListed();

    // Opens a web search for this job by company and title, for a posting
    // that has gone from the employer's board. It may be listed somewhere else.
    Q_INVOKABLE void searchTheWebForJob(qint64 jobApplicationId) const;

    // The user checked the job themselves and it is still open. The amber
    // "gone" mark comes off, and later checks leave this job alone.
    Q_INVOKABLE void userSaysJobIsStillOpen(qint64 jobApplicationId);

signals:
    void boardChanged();
    void lastActionChanged();
    void stillListedCheckChanged();

private:
    void setLastActionText(const QString &text);

    // Fills jobsThatCannotBeChecked. Asking the checker costs one database
    // read per card, so it is done once when the board loads, not once per
    // card every time the board is drawn. The board is drawn on every drag.
    void workOutWhichJobsCannotBeChecked();

    // The sort order saved for one column, or the default if none is saved.
    BoardColumnOrder::ColumnOrder columnOrderFor(const QString &stageName) const;

    JobPipelines &board;
    PostingStillListedChecker &postingChecker;
    ActiveJobsFolder &activeJobs;
    JobScout &scout;
    AppPreferences &preferences;
    int boardChangeCounter = 0;
    QString lastActionSentence;

    // Job posting id to "Job Crush has no link it can check for this job".
    // Worked out when the board loads. See workOutWhichJobsCannotBeChecked.
    QHash<qint64, bool> jobsThatCannotBeChecked;

    // The job the user asked about with "look for this job elsewhere". Only
    // that search reports on screen. The same search also runs by itself when
    // a job is crushed, and that one says nothing, because the user did not
    // ask for it. Zero means the user has not asked.
    qint64 jobPostingIdTheUserAskedAbout = 0;
};
