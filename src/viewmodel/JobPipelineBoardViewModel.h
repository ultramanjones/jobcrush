#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class JobPipelines;

// JobPipelineBoardViewModel
//
// Serves the board to QML: five columns of cards, and the handful of verbs
// that move them.
//
// Deliberately NOT a QAbstractListModel. Every other list in Job Crush is one,
// because every other list is one list. A board is five, and the honest way to
// serve five is to hand each column its own cards rather than give one model a
// filter role and make the view sort itself out. Columns are small — a job
// search with two hundred live applications is not a UI problem, it is a
// different life — so rebuilding a column's list is cheap and always correct.
//
// boardRevision is the same trick the credential roster uses: QML bindings
// that call cardsInStage() read the revision too, so they re-evaluate the
// moment anything underneath moves.
class JobPipelineBoardViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int boardRevision READ boardRevision NOTIFY boardChanged)
    Q_PROPERTY(int totalCardCount READ totalCardCount NOTIFY boardChanged)

    // What just happened, in one sentence, for the strip at the top of the
    // page. Empty when there is nothing to say.
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY lastActionChanged)

public:
    explicit JobPipelineBoardViewModel(JobPipelines &pipelines, QObject *parent = nullptr);

    int boardRevision() const;
    int totalCardCount() const;
    QString lastActionText() const;

    // The stages, in board order, as storage names: "saved", "applied", … The
    // view asks rather than hardcoding the list, so adding a stage one day
    // means changing PipelineStage and nothing else.
    Q_INVOKABLE QStringList stageNamesInBoardOrder() const;

    // The heading a column shows for a stage: "Saved", "Interviewing"…
    Q_INVOKABLE QString displayLabelForStage(const QString &stageName) const;

    // What a column is FOR, in one plain line. Shown under the heading while
    // the column is empty, because an empty column that says nothing is a
    // question mark, and Job Crush answers questions rather than posing them.
    Q_INVOKABLE QString explanationForStage(const QString &stageName) const;

    // The cards in one column, oldest crush first. Each is a plain object the
    // QML delegate reads by name: jobApplicationId, positionTitle,
    // companyName, locationText, salaryText, sourceUrl, isRemoteRole,
    // stageName, notesText, targetedText, appliedText.
    Q_INVOKABLE QVariantList cardsInStage(const QString &stageName) const;

    Q_INVOKABLE int countInStage(const QString &stageName) const;

    // --- The verbs --------------------------------------------------------

    // Drops a card into a column. Called by the DropArea under each column.
    Q_INVOKABLE void moveCardToStage(qint64 jobApplicationId, const QString &stageName);

    Q_INVOKABLE void setNotesFor(qint64 jobApplicationId, const QString &notesText);

    Q_INVOKABLE void removeCardFromBoard(qint64 jobApplicationId);

    // Opens the posting where it was published. The board links out; it never
    // pretends to host somebody else's listing.
    Q_INVOKABLE void openPostingInBrowser(qint64 jobApplicationId) const;

    Q_INVOKABLE void clearLastAction();

signals:
    void boardChanged();
    void lastActionChanged();

private:
    void setLastActionText(const QString &text);

    JobPipelines &board;
    int boardChangeCounter = 0;
    QString lastActionSentence;
};
