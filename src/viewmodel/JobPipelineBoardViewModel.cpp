#include "JobPipelineBoardViewModel.h"

#include <QDesktopServices>
#include <QLocale>
#include <QUrl>
#include <QVariantMap>

#include "../modelview/jobscout/AtsBoardIdentity.h"
#include "../modelview/jobscout/JobSourceDescriptor.h"
#include "../modelview/pipelines/JobPipelines.h"
#include "../modelview/pipelines/TypedDateReader.h"

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

// A route's board is either a job SITE Job Crush sweeps or an employer BOARD
// it reads. Two rosters, and asking only one is how a lowercase storage name
// reaches the screen.
static QString sourceDisplayNameFor(const QString &sourceStorageName)
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    if (descriptorFound) {
        return descriptor.displayName;
    }
    const QString boardDisplayName = AtsBoardName::displayNameFor(sourceStorageName);
    return boardDisplayName.isEmpty() ? sourceStorageName : boardDisplayName;
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
        card.insert(QStringLiteral("sourceName"),    targetedJob.posting.postingSource);
        card.insert(QStringLiteral("isRemoteRole"),  targetedJob.posting.isRemoteRole);
        card.insert(QStringLiteral("stageName"),     stageName);
        card.insert(QStringLiteral("notesText"),     targetedJob.campaign.notesText);
        card.insert(QStringLiteral("targetedText"),
                    shortDateTextFor(targetedJob.campaign.targetedTimestamp));
        card.insert(QStringLiteral("appliedText"),
                    shortDateTextFor(targetedJob.campaign.appliedTimestamp));

        // The applied date as the open card shows it for editing: "8 Oct 2026".
        // The year is in it, unlike the short text on the shut card, because
        // somebody correcting a date needs to see the whole date.
        card.insert(QStringLiteral("appliedDateEditText"),
                    TypedDateReader::editableDateTextFor(
                        targetedJob.campaign.appliedTimestamp.date()));

        // The date can be set on any card that has left Crushed, and on any
        // card that already has a date. A card still in Crushed with no date
        // has not been sent, and a date box there would invite a mistake.
        card.insert(QStringLiteral("appliedDateIsEditable"),
                    wantedStage != PipelineStage::Saved
                        || targetedJob.campaign.appliedTimestamp.isValid());

        // Every way of reaching this job, ready for the card to list. The
        // board name is put in front of the user already spelled properly —
        // a link reading "see this one on ashby" looks like a bug.
        QVariantList routes;
        for (const PostingSource &route :
                 board.routesToJobPosting(targetedJob.posting.jobPostingId)) {
            QVariantMap routeEntry;
            routeEntry.insert(QStringLiteral("boardDisplayName"),
                              sourceDisplayNameFor(route.boardName));
            routeEntry.insert(QStringLiteral("postingUrl"), route.postingUrl);
            routes.append(routeEntry);
        }
        card.insert(QStringLiteral("postingRoutes"), routes);

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
        setLastActionText(QStringLiteral("%1 moved to Applied with today's date. "
                                         "If you sent it on another day, open the card "
                                         "and change it.")
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

bool JobPipelineBoardViewModel::setAppliedDateFor(qint64 jobApplicationId,
                                                  const QString &typedDateText)
{
    QString positionTitle;
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == jobApplicationId) {
            positionTitle = targetedJob.posting.positionTitle;
            break;
        }
    }

    QString reasonText;

    // An empty box means "there is no applied date". That is the way back
    // for a card dragged to Applied by mistake.
    if (typedDateText.trimmed().isEmpty()) {
        if (!board.setAppliedDate(jobApplicationId, QDate(), reasonText)) {
            setLastActionText(reasonText);
            return false;
        }
        setLastActionText(QStringLiteral("%1 has no applied date now.").arg(positionTitle));
        return true;
    }

    QDate appliedDate;
    if (!TypedDateReader::readTypedDate(typedDateText, QDate::currentDate(), QLocale(),
                                        appliedDate)) {
        setLastActionText(QStringLiteral(
            "Job Crush couldn't read \"%1\" as a date, so the old one stays. "
            "Try 10/8, 8 Oct, yesterday, or 3 days ago.").arg(typedDateText.trimmed()));
        return false;
    }

    if (!board.setAppliedDate(jobApplicationId, appliedDate, reasonText)) {
        setLastActionText(reasonText);
        return false;
    }
    setLastActionText(QStringLiteral("%1: applied on %2.")
                          .arg(positionTitle,
                               TypedDateReader::editableDateTextFor(appliedDate)));
    return true;
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

void JobPipelineBoardViewModel::openPostingRouteInBrowser(const QString &postingUrl) const
{
    const QUrl routeUrl(postingUrl);
    // Only http and https. A stored string reaches this from the database and
    // handing anything else to the desktop is handing it whatever scheme the
    // string happens to carry.
    if (!routeUrl.isValid()
            || (routeUrl.scheme() != QStringLiteral("http")
                && routeUrl.scheme() != QStringLiteral("https"))) {
        return;
    }
    QDesktopServices::openUrl(routeUrl);
}
