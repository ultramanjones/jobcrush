#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "../../model/JobApplication.h"
#include "../../model/JobPosting.h"

class JobApplicationRepository;
class JobPostingRepository;

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
// The board, as a subject rather than as a screen — a ModelView resident.
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
// What does NOT live here: anything about columns, colors, dragging, or
// order on screen. Those are the view's business.
class JobPipelines : public QObject {
    Q_OBJECT
public:
    JobPipelines(JobApplicationRepository &applicationRepository,
                 JobPostingRepository &postingRepository,
                 QObject *parent = nullptr);

    // Reads the board in. Called once from the composition root and again
    // whenever something underneath has changed.
    void loadFromDatabase();

    // Every card, in board order (oldest crush first, so the board reads like
    // a history rather than reshuffling itself every time you look at it).
    QList<TargetedJob> everyTargetedJob() const;

    // How many cards sit in one stage. The column headers show this, and a
    // count that has to be recomputed by the view is a count that will
    // eventually disagree with the cards under it.
    int countInStage(PipelineStage pipelineStage) const;

    // --- Putting a job on the board ---------------------------------------

    // CRUSH. Puts a discovered posting on the board at Saved.
    //
    // Refuses politely if that posting is already up there: crushing the same
    // job twice is a slip, not an intention, and a second identical card
    // teaches the user that the board lies about how many irons are in the
    // fire. reasonText explains either outcome in words the user can read.
    bool crushJobPosting(qint64 jobPostingId, QString &reasonText);

    // Is this posting already on the board? The Discoveries list asks so it
    // can show "on your board" instead of an inviting button that will only
    // say no.
    bool jobPostingIsOnTheBoard(qint64 jobPostingId) const;

    // --- Moving through the stages ----------------------------------------

    // Moves a campaign. Arriving at Applied for the first time stamps the
    // date; going back does NOT clear it, because the fact that you applied
    // on the 3rd stays true even if you drag the card somewhere else.
    bool moveToStage(qint64 jobApplicationId, PipelineStage newPipelineStage);

    // The user's own running notes on one campaign.
    bool setNotesText(qint64 jobApplicationId, const QString &notesText);

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

    QList<TargetedJob> loadedTargetedJobs;
};
