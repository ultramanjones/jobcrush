#include "JobPipelineBoardViewModel.h"

#include <QDebug>
#include <QDesktopServices>
#include <QLocale>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

#include "../logic/activejobs/ActiveJobsFolder.h"
#include "../logic/jobscout/AtsBoardIdentity.h"
#include "../logic/jobscout/JobScout.h"
#include "../logic/jobscout/JobSourceDescriptor.h"
#include "../logic/pipelines/JobPipelines.h"
#include "../logic/pipelines/PostingStillListedChecker.h"
#include "../logic/pipelines/TypedDateReader.h"

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

// The line shown when the mouse rests on a column's heading. It says exactly
// when a card belongs in that column, so every user sorts their cards the same
// way and the Stats page counts the same thing for everyone.
QString hoverLineFor(PipelineStage pipelineStage)
{
    switch (pipelineStage) {
    case PipelineStage::Saved:
        return QStringLiteral("You picked this job but have not applied yet.");
    case PipelineStage::Applied:
        return QStringLiteral("You sent the application. It waits here until they "
                              "set up an interview.");
    case PipelineStage::Interview:
        return QStringLiteral("Move the job here only once you have a scheduled "
                              "interview. It stays here through every round until "
                              "an offer or a no.");
    case PipelineStage::Offer:
        return QStringLiteral("They made you an offer.");
    case PipelineStage::Closed:
        return QStringLiteral("It is over: they said no, you withdrew, or you turned "
                              "it down.");
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

JobPipelineBoardViewModel::JobPipelineBoardViewModel(JobPipelines &pipelines,
                                                     PostingStillListedChecker &stillListedChecker,
                                                     ActiveJobsFolder &activeJobsFolder,
                                                     JobScout &jobScout,
                                                     QObject *parent)
    : QObject(parent)
    , board(pipelines)
    , postingChecker(stillListedChecker)
    , activeJobs(activeJobsFolder)
    , scout(jobScout)
{
    // A card's folder path is part of the card. When a folder is made the
    // cards are served again so the link shows.
    connect(&activeJobs, &ActiveJobsFolder::folderStatusChanged, this, [this]() {
        ++boardChangeCounter;
        emit boardChanged();
    });

    connect(&postingChecker, &PostingStillListedChecker::checkStatusChanged,
            this, &JobPipelineBoardViewModel::stillListedCheckChanged);

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

bool JobPipelineBoardViewModel::stillListedCheckIsRunning() const
{
    return postingChecker.checkIsRunning();
}

QString JobPipelineBoardViewModel::stillListedCheckText() const
{
    return postingChecker.checkStatusText();
}

void JobPipelineBoardViewModel::checkWhichJobsAreStillListed()
{
    postingChecker.checkEveryOpenCard();
}

void JobPipelineBoardViewModel::searchTheWebForJob(qint64 jobApplicationId) const
{
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId != jobApplicationId) {
            continue;
        }
        QUrl searchUrl(QStringLiteral("https://www.google.com/search"));
        QUrlQuery searchQuery;
        searchQuery.addQueryItem(QStringLiteral("q"),
                                 QStringLiteral("\"%1\" \"%2\" job")
                                     .arg(targetedJob.posting.companyName,
                                          targetedJob.posting.positionTitle));
        searchUrl.setQuery(searchQuery);
        if (!QDesktopServices::openUrl(searchUrl)) {
            qWarning().noquote() << QStringLiteral(
                "JobPipelineBoardViewModel::searchTheWebForJob: could not open the web "
                "search for job application %1 — the system browser refused it")
                    .arg(jobApplicationId);
        }
        return;
    }
    qWarning().noquote() << QStringLiteral(
        "JobPipelineBoardViewModel::searchTheWebForJob: job application %1 is not on "
        "the board").arg(jobApplicationId);
}

void JobPipelineBoardViewModel::userSaysJobIsStillOpen(qint64 jobApplicationId)
{
    // The board reloads when this works, and the card redraws without the
    // amber mark. A failure is logged where it happens.
    board.markStillOpenByUser(jobApplicationId);
}

void JobPipelineBoardViewModel::lookForOtherPlacesToApply(qint64 jobApplicationId)
{
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == jobApplicationId) {
            scout.lookForOtherPlacesToApply(targetedJob.posting.jobPostingId);
            setLastActionText(QStringLiteral(
                "Looking for %1 on %2's own board. If it is there, a new link shows on "
                "the card.").arg(targetedJob.posting.positionTitle,
                                 targetedJob.posting.companyName));
            return;
        }
    }
    qWarning().noquote() << QStringLiteral(
        "JobPipelineBoardViewModel::lookForOtherPlacesToApply: job application %1 is "
        "not on the board").arg(jobApplicationId);
}

void JobPipelineBoardViewModel::openJobFolder(qint64 jobApplicationId) const
{
    const QString jobFolderPath = activeJobs.jobFolderPathForCard(jobApplicationId);
    if (jobFolderPath.isEmpty()) {
        return; // this card has no job folder yet, so there is nothing to open
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(jobFolderPath))) {
        qWarning().noquote() << QStringLiteral(
            "JobPipelineBoardViewModel::openJobFolder: could not open \"%1\" for job "
            "application %2 — the system file manager refused it")
                .arg(jobFolderPath).arg(jobApplicationId);
    }
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

QString JobPipelineBoardViewModel::hoverLineForStage(const QString &stageName) const
{
    return hoverLineFor(pipelineStageFromStorageText(stageName));
}

// The name to show for the board a route is on. A board is either a job SITE
// Job Crush sweeps or an employer BOARD it reads. They are listed in two
// different places, so both are asked. Asking only one would put a lowercase
// storage name such as "ashby" on screen.
static QString sourceDisplayNameFor(const QString &sourceStorageName)
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    if (descriptorFound) {
        return descriptor.displayName;
    }
    const QString boardDisplayName = AtsBoardName::displayNameFor(sourceStorageName);
    if (!boardDisplayName.isEmpty()) {
        return boardDisplayName;
    }
    // A site Job Crush has no name for, such as one an outside AI wrote in a
    // JobQuest.txt: shown as written, with a capital.
    return sourceStorageName.left(1).toUpper() + sourceStorageName.mid(1);
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
        card.insert(QStringLiteral("sourceUrl"),     targetedJob.posting.sourceUrl);
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

        // A posting that has gone from the employer's board. The card says
        // so in plain words and offers a search. It is not removed: the job
        // may be listed somewhere else, and that is the user's call.
        const QDateTime goneSince = targetedJob.posting.postingGoneSinceTimestamp;
        card.insert(QStringLiteral("postingSeemsGone"), goneSince.isValid());
        card.insert(QStringLiteral("postingGoneText"),
                    goneSince.isValid()
                        ? QStringLiteral("The company's own board stopped listing this job "
                                         "on %1. Check whether it's still listed somewhere "
                                         "else.")
                              .arg(shortDateTextFor(goneSince))
                        : QString());

        // The user checked the job themselves and said it is still open.
        const QDateTime userSaysOpenSince = targetedJob.posting.userSaysStillOpenTimestamp;
        card.insert(QStringLiteral("userSaysStillOpen"), userSaysOpenSince.isValid());
        card.insert(QStringLiteral("userSaysStillOpenText"),
                    userSaysOpenSince.isValid()
                        ? QStringLiteral("You checked this job on %1: still open. Job Crush "
                                         "won't mark it gone.")
                              .arg(shortDateTextFor(userSaysOpenSince))
                        : QString());

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

        // The job's own folder, where its JobQuest.txt, resume and cover
        // letter live. Empty when it has none yet.
        card.insert(QStringLiteral("jobFolderPath"),
                    activeJobs.jobFolderPathForCard(targetedJob.campaign.jobApplicationId));

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
        if (targetedJob.posting.sourceUrl.isEmpty()) {
            return; // a hand-added job with no link has nothing to open
        }
        // Only http and https, for the same reason as in
        // openPostingRouteInBrowser below.
        const QUrl postingUrl(targetedJob.posting.sourceUrl);
        if (!postingUrl.isValid()
                || (postingUrl.scheme() != QStringLiteral("http")
                    && postingUrl.scheme() != QStringLiteral("https"))) {
            qWarning().noquote() << QStringLiteral(
                "JobPipelineBoardViewModel::openPostingInBrowser: refused to open \"%1\" "
                "for job application %2, because it is not a web link")
                    .arg(targetedJob.posting.sourceUrl).arg(jobApplicationId);
            return;
        }
        if (!QDesktopServices::openUrl(postingUrl)) {
            qWarning().noquote() << QStringLiteral(
                "JobPipelineBoardViewModel::openPostingInBrowser: could not open \"%1\" "
                "for job application %2 — the system browser refused it")
                    .arg(targetedJob.posting.sourceUrl).arg(jobApplicationId);
        }
        return;
    }
    qWarning().noquote() << QStringLiteral(
        "JobPipelineBoardViewModel::openPostingInBrowser: job application %1 is not on "
        "the board").arg(jobApplicationId);
}

void JobPipelineBoardViewModel::openPostingRouteInBrowser(const QString &postingUrl) const
{
    const QUrl routeUrl(postingUrl);
    // Only http and https. The link comes from the database, not from the
    // user. A link like file:// or a program's own scheme could make the
    // computer do something other than show a web page.
    if (!routeUrl.isValid()
            || (routeUrl.scheme() != QStringLiteral("http")
                && routeUrl.scheme() != QStringLiteral("https"))) {
        qWarning().noquote() << QStringLiteral(
            "JobPipelineBoardViewModel::openPostingRouteInBrowser: refused to open "
            "\"%1\", because it is not a web link").arg(postingUrl);
        return;
    }
    if (!QDesktopServices::openUrl(routeUrl)) {
        qWarning().noquote() << QStringLiteral(
            "JobPipelineBoardViewModel::openPostingRouteInBrowser: could not open \"%1\" "
            "— the system browser refused it").arg(postingUrl);
    }
}
