#pragma once

#include <QDate>
#include <QList>
#include <QObject>
#include <QString>

#include "../../model/JobApplication.h"
#include "../../model/JobPosting.h"
#include "../../model/PostingSource.h"

class JobApplicationRepository;
class JobPostingRepository;
class PostingSourceRepository;

// TargetedJob
//
// One card on the board: the campaign and the job posting it targets, already
// joined. Nothing above this layer should have to look up the posting
// separately. A card missing its company name because a second lookup failed
// is a bug the user sees.
struct TargetedJob {
    JobApplication campaign;
    JobPosting posting;
};

// JobPipelines
//
// The board's data and rules, with nothing about how it looks. A Logic layer
// class.
//
// Named plural on purpose. A job search is not one funnel that you feed and
// wait beside; it is a dozen of them running at once, at different speeds,
// and the whole point of looking at this screen is seeing that you have
// feelers out in every direction rather than one hope you are sitting on.
//
// What lives here:
//  - the rules about MOVING between stages, including the ones with side
//    effects (arriving at Applied stamps the date, because the date is the
//    thing people forget and then cannot answer "when did I apply?"),
//  - crushing a discovered posting onto the board without letting it land
//    twice,
//  - the joined view of campaign-plus-posting the board is made of,
//  - the counts each column shows.
//
// What does NOT live here: anything about columns, colors or dragging.
// Those are the view's business. The order of the cards inside one column is
// worked out by BoardColumnOrder, which is also in this folder.
class JobPipelines : public QObject {
    Q_OBJECT
public:
    JobPipelines(JobApplicationRepository &applicationRepository,
                 JobPostingRepository &postingRepository,
                 PostingSourceRepository &postingSourceRepository,
                 QObject *parent = nullptr);

    // Reads the board in. Called once from the composition root and again
    // whenever something underneath has changed.
    void loadFromDatabase();

    // Every card, in board order (oldest crush first, so the board reads like
    // a history rather than reshuffling itself every time you look at it).
    QList<TargetedJob> everyTargetedJob() const;

    // The user checked this card's job themselves and it is still open.
    // Takes off any "gone" mark, and Job Crush never marks the job gone
    // again. Returns false when the card is not on the board or the save
    // failed (the repository logs why).
    bool markStillOpenByUser(qint64 jobApplicationId);

    // Every card that is not in Closed or Escaped, in the same order. The
    // search for those jobs is over, so pages like Staging leave them out,
    // and the "are these still open?" check does not ask about them.
    QList<TargetedJob> everyOpenTargetedJob() const;

    // Every way of reaching one job. The card shows these so the user picks
    // the route rather than the app picking it for them.
    QList<PostingSource> routesToJobPosting(qint64 jobPostingId) const;

    // How many cards sit in one stage. The column headers show this, and a
    // count that has to be recomputed by the view is a count that will
    // eventually disagree with the cards under it.
    int countInStage(PipelineStage pipelineStage) const;

    // --- Putting a job on the board ---------------------------------------

    // CRUSH. Puts a discovered posting on the board at Saved.
    //
    // Refuses politely if that posting is already up there: crushing the same
    // job twice is a slip, not an intention, and a second identical card
    // would make the board show more jobs in play than there are. reasonText
    // explains either outcome in words the user can read.
    bool crushJobPosting(qint64 jobPostingId, QString &reasonText);

    // Puts a job from a job folder on the board, in the column the folder's
    // JobQuest.txt names, with the applied date and notes the file gave.
    // If the column is Applied, Interview or Offer and appliedTimestamp is
    // empty, then the card is stamped with the date and time right now.
    //
    // Different from CRUSH on purpose. A packet is only started when the job
    // lands in Crushed. A job that was already sent needs no cover letter, and
    // starting a packet asks the brain questions, which spends the user's AI
    // credits for nothing.
    //
    // Returns the new card's id, or 0 with the reason in reasonText.
    qint64 putOnBoardFromJobFolder(qint64 jobPostingId, PipelineStage pipelineStage,
                                   const QDateTime &appliedTimestamp,
                                   const QString &notesText, QString &reasonText);

    // Close from Discoveries. Puts a job in the Closed column, so the board
    // keeps a record of it: the user looked at it, and it is over.
    //
    // If the job is not on the board, a new card is made straight in Closed.
    // No packet is started, because nothing will be sent. The card has no
    // applied date. The user can type one on the card.
    //
    // If the job is already on the board, its card moves to Closed. Its
    // applied date and notes stay.
    //
    // reasonText says what happened, in words the user can read. Returns
    // false when nothing changed.
    bool closeJobPosting(qint64 jobPostingId, QString &reasonText);

    // The card for one posting, or 0 when that posting is not on the board.
    qint64 jobApplicationIdForPosting(qint64 jobPostingId) const;

    // Is this posting already on the board? The Discoveries list asks so it
    // can show "on your board" instead of an inviting button that will only
    // say no.
    bool jobPostingIsOnTheBoard(qint64 jobPostingId) const;

    // --- Moving through the stages ----------------------------------------

    // Moves a campaign. Arriving at Applied, Interview or Offer with no
    // applied date stamps the date and time right now. Going back does NOT
    // clear it, because the fact that you applied on the 3rd stays true even
    // if you drag the card somewhere else.
    bool moveToStage(qint64 jobApplicationId, PipelineStage newPipelineStage);

    // The user's own running notes on one campaign.
    bool setNotesText(qint64 jobApplicationId, const QString &notesText);

    // Sets the day the user applied, by hand. People do not always drag the
    // card on the day they send the application, and the date the board
    // stamped is then wrong. The user knows better than the app, so their date
    // replaces it. An invalid date clears it, for a card that reached Applied
    // by mistake.
    //
    // Refuses a date in the future and says why in reasonText. Nobody has
    // applied tomorrow, so a future date is a typo, and every count on the
    // Stats page would be built on it.
    bool setAppliedDate(qint64 jobApplicationId, const QDate &appliedDate,
                        QString &reasonText);

    // Takes a campaign off the board entirely. The POSTING stays — it was
    // never Job Crush's to delete, and it belongs in Discoveries either way.
    bool removeFromBoard(qint64 jobApplicationId);

signals:
    // The board changed: a card arrived, moved, or left.
    void boardChanged();

    // A job was just crushed onto the board. Carries the new campaign id.
    //
    // Separate from boardChanged because they mean different things:
    // boardChanged means redraw, this means a new campaign started.
    // StagingWorkbench listens for this and starts the packet. It only fires
    // on a real crush, never when a duplicate crush is refused.
    void jobWasCrushed(qint64 jobApplicationId);

private:
    JobApplicationRepository &campaignRepository;
    JobPostingRepository &postingRepository;
    PostingSourceRepository &jobRouteRepository;

    // Every card, as loadFromDatabase last read them.
    QList<TargetedJob> loadedTargetedJobs;
};
