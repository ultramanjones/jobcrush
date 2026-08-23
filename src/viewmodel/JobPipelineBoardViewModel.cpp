#include "JobPipelineBoardViewModel.h"

#include <QDesktopServices>
#include <QLocale>
#include <QUrl>
#include <QVariantMap>

#include "../modelview/pipelines/JobPipelines.h"

namespace {

// The board's columns, left to right. One list, read by the view.
const PipelineStage stagesInBoardOrder[] = {
    PipelineStage::Saved,
    PipelineStage::Applied,
    PipelineStage::Interview,
    PipelineStage::Offer,
    PipelineStage::Closed,
};

// What each column is called on screen. Not the storage word: "saved" is how
// it is written down, "Crushed" is what it means to the person looking at it.
QString displayLabelFor(PipelineStage pipelineStage)
{
    switch (pipelineStage) {
    case PipelineStage::Saved:     return QStringLiteral("Crushed");
    case PipelineStage::Applied:   return QStringLiteral("Applied");
    case PipelineStage::Interview: return QStringLiteral("Interviewing");
    case PipelineStage::Offer:     return QStringLiteral("Offer");
    case PipelineStage::Closed:    return QStringLiteral("Closed");
    }
    return QString();
}

// What a column is for. Shown while it is empty — nobody should have to guess
// what belongs in a box, and "Offer" with nothing under it tells you nothing.
QString explanationFor(PipelineStage pipelineStage)
{
    switch (pipelineStage) {
    case PipelineStage::Saved:
        return QStringLiteral("Jobs you want. Nothing has been sent yet — this is the "
                              "pile you work from.");
    case PipelineStage::Applied:
        return QStringLiteral("You've sent it. Drag a card here the day you apply and "
                              "Job Crush remembers the date for you.");
    case PipelineStage::Interview:
        return QStringLiteral("Somebody wants to talk. Anything from a screening call "
                              "upwards belongs here.");
    case PipelineStage::Offer:
        return QStringLiteral("An offer is on the table. This is the column the whole "
                              "board exists to fill.");
    case PipelineStage::Closed:
        return QStringLiteral("Finished — turned down, withdrawn, or you said no. "
                              "Keeping them is how you see how far you got.");
    }
    return QString();
}

// "3 Feb" — short, because a card is a glance, not a record.
QString shortDateTextFor(const QDateTime &timestamp)
{
    if (!timestamp.isValid()) {
        return QString();
    }
    return QLocale().toString(timestamp.date(), QStringLiteral("d MMM"));
}

} // namespace

JobPipelineBoardViewModel::JobPipelineBoardViewModel(JobPipelines &pipelines, QObject *parent)
    : QObject(parent)
    , board(pipelines)
{
    connect(&board, &JobPipelines::boardChanged, this, [this]() {
        ++boardChangeCounter;
        emit boardChanged();
    });
}

int JobPipelineBoardViewModel::boardRevision() const
{
    return boardChangeCounter;
}

int JobPipelineBoardViewModel::totalCardCount() const
{
    return board.everyTargetedJob().count();
}

QString JobPipelineBoardViewModel::lastActionText() const
{
    return lastActionSentence;
}

void JobPipelineBoardViewModel::setLastActionText(const QString &text)
{
    if (lastActionSentence == text) {
        return;
    }
    lastActionSentence = text;
    emit lastActionChanged();
}

void JobPipelineBoardViewModel::clearLastAction()
{
    setLastActionText(QString());
}

QStringList JobPipelineBoardViewModel::stageNamesInBoardOrder() const
{
    QStringList stageNames;
    for (PipelineStage pipelineStage : stagesInBoardOrder) {
        stageNames.append(pipelineStageToStorageText(pipelineStage));
    }
    return stageNames;
}

QString JobPipelineBoardViewModel::displayLabelForStage(const QString &stageName) const
{
    return displayLabelFor(pipelineStageFromStorageText(stageName));
}

QString JobPipelineBoardViewModel::explanationForStage(const QString &stageName) const
{
    return explanationFor(pipelineStageFromStorageText(stageName));
}

QVariantList JobPipelineBoardViewModel::cardsInStage(const QString &stageName) const
{
    const PipelineStage wantedStage = pipelineStageFromStorageText(stageName);

    QVariantList cards;
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.pipelineStage != wantedStage) {
            continue;
        }

        QVariantMap card;
        card.insert(QStringLiteral("jobApplicationId"),
                    QVariant::fromValue(targetedJob.campaign.jobApplicationId));
        card.insert(QStringLiteral("positionTitle"), targetedJob.posting.positionTitle);
        card.insert(QStringLiteral("companyName"),   targetedJob.posting.companyName);
        card.insert(QStringLiteral("locationText"),  targetedJob.posting.locationText);
        card.insert(QStringLiteral("salaryText"),    targetedJob.posting.salaryText);
        card.insert(QStringLiteral("sourceName"),    targetedJob.posting.discoverySource);
        card.insert(QStringLiteral("isRemoteRole"),  targetedJob.posting.isRemoteRole);
        card.insert(QStringLiteral("stageName"),     stageName);
        card.insert(QStringLiteral("notesText"),     targetedJob.campaign.notesText);
        card.insert(QStringLiteral("targetedText"),
                    shortDateTextFor(targetedJob.campaign.targetedTimestamp));
        card.insert(QStringLiteral("appliedText"),
                    shortDateTextFor(targetedJob.campaign.appliedTimestamp));
        cards.append(card);
    }
    return cards;
}

int JobPipelineBoardViewModel::countInStage(const QString &stageName) const
{
    return board.countInStage(pipelineStageFromStorageText(stageName));
}

void JobPipelineBoardViewModel::moveCardToStage(qint64 jobApplicationId,
                                                const QString &stageName)
{
    const PipelineStage newStage = pipelineStageFromStorageText(stageName);

    // Find the card first, so the sentence afterwards can name the job rather
    // than saying "moved" and leaving the user to work out which one.
    QString positionTitle;
    bool alreadyInThatStage = false;
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId != jobApplicationId) {
            continue;
        }
        positionTitle = targetedJob.posting.positionTitle;
        alreadyInThatStage = targetedJob.campaign.pipelineStage == newStage;
        break;
    }

    if (alreadyInThatStage) {
        return; // dropped back where it started; nothing happened, say nothing
    }

    if (!board.moveToStage(jobApplicationId, newStage)) {
        setLastActionText(QStringLiteral(
            "That card wouldn't move. It's still where it was — try dragging it "
            "again, and if it keeps refusing, restarting Job Crush clears it."));
        return;
    }

    if (newStage == PipelineStage::Applied) {
        setLastActionText(QStringLiteral("%1 moved to Applied — today's date is on the card.")
                              .arg(positionTitle));
        return;
    }
    setLastActionText(QStringLiteral("%1 moved to %2.")
                          .arg(positionTitle, displayLabelFor(newStage)));
}

void JobPipelineBoardViewModel::setNotesFor(qint64 jobApplicationId, const QString &notesText)
{
    board.setNotesText(jobApplicationId, notesText);
}

void JobPipelineBoardViewModel::removeCardFromBoard(qint64 jobApplicationId)
{
    QString positionTitle;
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == jobApplicationId) {
            positionTitle = targetedJob.posting.positionTitle;
            break;
        }
    }

    if (!board.removeFromBoard(jobApplicationId)) {
        setLastActionText(QStringLiteral("That card wouldn't come off the board. "
                                         "Nothing has changed."));
        return;
    }
    setLastActionText(QStringLiteral(
        "%1 is off the board. The job itself is still on Discoveries if you want it back.")
            .arg(positionTitle));
}

void JobPipelineBoardViewModel::openPostingInBrowser(qint64 jobApplicationId) const
{
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId != jobApplicationId) {
            continue;
        }
        if (!targetedJob.posting.sourceUrl.isEmpty()) {
            QDesktopServices::openUrl(QUrl(targetedJob.posting.sourceUrl));
        }
        return;
    }
}
