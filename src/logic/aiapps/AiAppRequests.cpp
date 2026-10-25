#include "AiAppRequests.h"

#include <QDebug>
#include <QDir>
#include <QList>
#include <QStringList>

#include "../../model/JobPostingRepository.h"
#include "../activejobs/ActiveJobsFolder.h"
#include "../activejobs/JobQuestFile.h"
#include "../jobscout/DuplicateJobDetector.h"
#include "../pipelines/JobPipelines.h"
#include "AiAppTools.h"

namespace {

// An application sent this many days ago with no news is due a follow-up.
constexpr int daysBeforeFollowUp = 14;

// A typed applied date has no time of day, so it is stored at noon. Noon
// keeps the date the same in any time zone a few hours away.
const QTime appliedDateTimeOfDay = QTime(12, 0);

AiAppAnswer errorAnswer(const QString &text)
{
    return { text, true };
}

// One argument as trimmed text. A number the AI sent is turned into text,
// and a missing argument is empty.
QString argumentText(const QJsonObject &arguments, const QString &argumentName)
{
    return arguments.value(argumentName).toVariant().toString().trimmed();
}

// Finds the card whose number is the job_id argument and copies it into
// cardFound. Returns false when the number is missing or no card has it.
bool findCard(const JobPipelines &board, const QJsonObject &arguments, TargetedJob &cardFound)
{
    const qint64 jobId = arguments.value(QStringLiteral("job_id")).toVariant().toLongLong();
    if (jobId <= 0) {
        return false;
    }
    for (const TargetedJob &targetedJob : board.everyTargetedJob()) {
        if (targetedJob.campaign.jobApplicationId == jobId) {
            cardFound = targetedJob;
            return true;
        }
    }
    return false;
}

QString noSuchJobText()
{
    return QStringLiteral("There is no job with that number on the board. Call list_jobs "
                          "to see the numbers.");
}

// One job as one line: "#12 | Acme | Qt Dev | Applied | applied 10/14/2026".
// Notes and the folder, when there are any, go on indented lines below it.
QString lineForCard(const TargetedJob &card, const ActiveJobsFolder &activeJobs)
{
    QStringList lineFields;
    lineFields.append(QStringLiteral("#%1").arg(card.campaign.jobApplicationId));
    lineFields.append(card.posting.companyName);
    lineFields.append(card.posting.positionTitle);
    lineFields.append(JobQuestFile::statusWordFor(card.campaign.pipelineStage));
    if (card.campaign.appliedTimestamp.isValid()) {
        lineFields.append(QStringLiteral("applied %1")
                              .arg(JobQuestFile::appliedOnTextFor(card.campaign.appliedTimestamp.date())));
    }
    if (!card.posting.locationText.isEmpty()) {
        lineFields.append(card.posting.locationText);
    }
    if (!card.posting.salaryText.isEmpty()) {
        lineFields.append(QStringLiteral("pay %1").arg(card.posting.salaryText));
    }
    if (!card.posting.sourceUrl.isEmpty()) {
        lineFields.append(card.posting.sourceUrl);
    }
    QString line = lineFields.join(QStringLiteral(" | "));
    if (!card.campaign.notesText.trimmed().isEmpty()) {
        QString notes = card.campaign.notesText.trimmed();
        notes.replace(QLatin1Char('\n'), QStringLiteral(" / "));
        line += QStringLiteral("\n    notes: ") + notes;
    }
    const QString folderPath = activeJobs.jobFolderPathForCard(card.campaign.jobApplicationId);
    if (!folderPath.isEmpty()) {
        line += QStringLiteral("\n    folder: ") + QDir::toNativeSeparators(folderPath);
    }
    return line;
}

// Reads a typed date. Empty text is not a date.
bool readDate(const QString &dateText, const QDate &today, QDate &dateRead)
{
    return !dateText.isEmpty() && JobQuestFile::readAppliedOn(dateText, today, dateRead);
}

} // namespace

AiAppRequests::AiAppRequests(JobPipelines &pipelines,
                             JobPostingRepository &postingRepositoryToUse,
                             ActiveJobsFolder &activeJobsFolder)
    : board(pipelines)
    , postingRepository(postingRepositoryToUse)
    , activeJobs(activeJobsFolder)
{
}

AiAppAnswer AiAppRequests::runAction(const QString &actionName, const QJsonObject &arguments,
                                     const QDate &today) const
{
    if (actionName == AiAppTools::ListJobs) {
        return listJobs(arguments);
    }
    if (actionName == AiAppTools::AddJob) {
        return addJob(arguments, today);
    }
    if (actionName == AiAppTools::MarkApplied) {
        return markApplied(arguments, today);
    }
    if (actionName == AiAppTools::MoveJob) {
        return moveJob(arguments);
    }
    if (actionName == AiAppTools::AddNote) {
        return addNote(arguments, today);
    }
    if (actionName == AiAppTools::WhatsWaiting) {
        return whatsWaiting(today);
    }
    return errorAnswer(QStringLiteral("Job Crush has no action named \"%1\". Its actions are "
                                      "list_jobs, add_job, mark_applied, move_job, add_note "
                                      "and whats_waiting.").arg(actionName));
}

AiAppAnswer AiAppRequests::listJobs(const QJsonObject &arguments) const
{
    const QString columnText = argumentText(arguments, QStringLiteral("column"));
    PipelineStage wantedStage = PipelineStage::Saved;
    const bool filterByColumn = !columnText.isEmpty();
    if (filterByColumn && !JobQuestFile::readStatus(columnText, wantedStage)) {
        return errorAnswer(QStringLiteral("\"%1\" is not a column. The columns are Saved, "
                                          "Applied, Interview, Offer and Closed.")
                               .arg(columnText));
    }
    const QString searchText = argumentText(arguments, QStringLiteral("search"));

    QStringList lines;
    for (const TargetedJob &card : board.everyTargetedJob()) {
        if (filterByColumn && card.campaign.pipelineStage != wantedStage) {
            continue;
        }
        if (!searchText.isEmpty()
                && !card.posting.companyName.contains(searchText, Qt::CaseInsensitive)
                && !card.posting.positionTitle.contains(searchText, Qt::CaseInsensitive)) {
            continue;
        }
        lines.append(lineForCard(card, activeJobs));
    }
    if (lines.isEmpty()) {
        return { QStringLiteral("No jobs on the board match that."), false };
    }
    return { QStringLiteral("%1 jobs:\n").arg(lines.count()) + lines.join(QLatin1Char('\n')),
             false };
}

AiAppAnswer AiAppRequests::addJob(const QJsonObject &arguments, const QDate &today) const
{
    JobPosting newPosting;
    newPosting.companyName = argumentText(arguments, QStringLiteral("company"));
    newPosting.positionTitle = argumentText(arguments, QStringLiteral("title"));
    if (newPosting.companyName.isEmpty() || newPosting.positionTitle.isEmpty()) {
        return errorAnswer(QStringLiteral("add_job needs both company and title."));
    }
    newPosting.sourceUrl = argumentText(arguments, QStringLiteral("link"));
    newPosting.postingSource = argumentText(arguments, QStringLiteral("found_on"));
    newPosting.salaryText = argumentText(arguments, QStringLiteral("pay"));
    newPosting.locationText = argumentText(arguments, QStringLiteral("where"));
    newPosting.isRemoteRole =
        newPosting.locationText.contains(QStringLiteral("remote"), Qt::CaseInsensitive);
    const QString createdBy = argumentText(arguments, QStringLiteral("created_by"));
    newPosting.scoutSource = createdBy.isEmpty() ? QStringLiteral("AI app") : createdBy;
    newPosting.discoveredTimestamp = QDateTime::currentDateTime();

    PipelineStage stage = PipelineStage::Saved;
    const QString columnText = argumentText(arguments, QStringLiteral("column"));
    // readStatus fills stage only when it can read the column word.
    if (!columnText.isEmpty() && !JobQuestFile::readStatus(columnText, stage)) {
        return errorAnswer(QStringLiteral("\"%1\" is not a column. The columns are Saved, "
                                          "Applied, Interview, Offer and Closed. Nothing "
                                          "was added.").arg(columnText));
    }

    const QString appliedText = argumentText(arguments, QStringLiteral("applied_on"));
    QDate appliedDate;
    if (!appliedText.isEmpty() && !readDate(appliedText, today, appliedDate)) {
        return errorAnswer(QStringLiteral("Job Crush could not read \"%1\" as a date. Write "
                                          "it like 10/14/2026. Nothing was added.")
                               .arg(appliedText));
    }
    if (appliedDate.isValid() && appliedDate > today) {
        return errorAnswer(QStringLiteral("The applied date is in the future. Nothing was "
                                          "added."));
    }
    // An applied date with no column means the job was applied to. Any
    // column past Saved means it was applied to, so a missing date is today.
    if (columnText.isEmpty() && appliedDate.isValid()) {
        stage = PipelineStage::Applied;
    }
    if (stage != PipelineStage::Saved && !appliedDate.isValid()) {
        appliedDate = today;
    }

    // The same job may already be known: found by a sweep, added by hand, or
    // added by an AI before. Then it is reused, never added twice.
    const DuplicateJobDetector duplicateDetector;
    qint64 jobPostingId = 0;
    for (const JobPosting &knownPosting : postingRepository.loadAllJobPostings()) {
        if (duplicateDetector.sameJob(knownPosting, newPosting)) {
            jobPostingId = knownPosting.jobPostingId;
            break;
        }
    }
    if (jobPostingId != 0) {
        const qint64 existingCardId = board.jobApplicationIdForPosting(jobPostingId);
        if (existingCardId != 0) {
            return { QStringLiteral("That job is already on the board as #%1. Nothing was "
                                    "added. Use mark_applied, move_job or add_note to "
                                    "change it.").arg(existingCardId),
                     false };
        }
    } else {
        if (!postingRepository.insertJobPosting(newPosting)) {
            qWarning().noquote() << QStringLiteral(
                "AiAppRequests::addJob: could not save \"%1\" at \"%2\" — %3")
                .arg(newPosting.positionTitle, newPosting.companyName,
                     postingRepository.lastErrorText());
            return errorAnswer(QStringLiteral("Job Crush could not save the job: %1")
                                   .arg(postingRepository.lastErrorText()));
        }
        jobPostingId = newPosting.jobPostingId;
    }

    QString reasonText;
    const qint64 newCardId = board.putOnBoardFromJobFolder(
        jobPostingId, stage,
        appliedDate.isValid() ? QDateTime(appliedDate, appliedDateTimeOfDay) : QDateTime(),
        argumentText(arguments, QStringLiteral("notes")), reasonText);
    if (newCardId == 0) {
        return errorAnswer(reasonText);
    }

    QString answer = QStringLiteral("Added %1 at %2 to the board as #%3, in %4.")
                         .arg(newPosting.positionTitle, newPosting.companyName)
                         .arg(newCardId)
                         .arg(JobQuestFile::statusWordFor(stage));
    const QString folderPath = activeJobs.jobFolderPathForCard(newCardId);
    if (!folderPath.isEmpty()) {
        answer += QStringLiteral(" Its folder is %1. Save any resume or cover letter for "
                                 "it there.").arg(QDir::toNativeSeparators(folderPath));
    }
    return { answer, false };
}

AiAppAnswer AiAppRequests::markApplied(const QJsonObject &arguments, const QDate &today) const
{
    TargetedJob card;
    if (!findCard(board, arguments, card)) {
        return errorAnswer(noSuchJobText());
    }

    const QString appliedText = argumentText(arguments, QStringLiteral("applied_on"));
    QDate appliedDate = today;
    if (!appliedText.isEmpty() && !readDate(appliedText, today, appliedDate)) {
        return errorAnswer(QStringLiteral("Job Crush could not read \"%1\" as a date. Write "
                                          "it like 10/14/2026. Nothing changed.")
                               .arg(appliedText));
    }

    const qint64 cardId = card.campaign.jobApplicationId;
    if (card.campaign.pipelineStage != PipelineStage::Applied
            && !board.moveToStage(cardId, PipelineStage::Applied)) {
        qWarning().noquote() << QStringLiteral(
            "AiAppRequests::markApplied: could not move card #%1 to Applied").arg(cardId);
        return errorAnswer(QStringLiteral("Job Crush could not move the job. Nothing "
                                          "changed."));
    }
    QString reasonText;
    if (!board.setAppliedDate(cardId, appliedDate, reasonText)) {
        return errorAnswer(reasonText);
    }
    return { QStringLiteral("#%1, %2 at %3, is now Applied, sent %4.")
                 .arg(cardId)
                 .arg(card.posting.positionTitle, card.posting.companyName,
                      JobQuestFile::appliedOnTextFor(appliedDate)),
             false };
}

AiAppAnswer AiAppRequests::moveJob(const QJsonObject &arguments) const
{
    TargetedJob card;
    if (!findCard(board, arguments, card)) {
        return errorAnswer(noSuchJobText());
    }
    const QString columnText = argumentText(arguments, QStringLiteral("column"));
    PipelineStage stage = PipelineStage::Saved;
    if (!JobQuestFile::readStatus(columnText, stage)) {
        return errorAnswer(QStringLiteral("\"%1\" is not a column. The columns are Saved, "
                                          "Applied, Interview, Offer and Closed.")
                               .arg(columnText));
    }
    if (!board.moveToStage(card.campaign.jobApplicationId, stage)) {
        qWarning().noquote() << QStringLiteral(
            "AiAppRequests::moveJob: could not move card #%1 to %2")
            .arg(card.campaign.jobApplicationId)
            .arg(JobQuestFile::statusWordFor(stage));
        return errorAnswer(QStringLiteral("Job Crush could not move the job. Nothing "
                                          "changed."));
    }
    return { QStringLiteral("#%1, %2 at %3, is now in %4.")
                 .arg(card.campaign.jobApplicationId)
                 .arg(card.posting.positionTitle, card.posting.companyName,
                      JobQuestFile::statusWordFor(stage)),
             false };
}

AiAppAnswer AiAppRequests::addNote(const QJsonObject &arguments, const QDate &today) const
{
    TargetedJob card;
    if (!findCard(board, arguments, card)) {
        return errorAnswer(noSuchJobText());
    }
    const QString noteText = argumentText(arguments, QStringLiteral("note"));
    if (noteText.isEmpty()) {
        return errorAnswer(QStringLiteral("add_note needs the note text."));
    }

    const QString datedNote = JobQuestFile::appliedOnTextFor(today) + QStringLiteral(": ")
                              + noteText;
    const QString existingNotes = card.campaign.notesText.trimmed();
    const QString allNotes = existingNotes.isEmpty()
        ? datedNote
        : existingNotes + QLatin1Char('\n') + datedNote;
    if (!board.setNotesText(card.campaign.jobApplicationId, allNotes)) {
        qWarning().noquote() << QStringLiteral(
            "AiAppRequests::addNote: could not save the note on card #%1")
            .arg(card.campaign.jobApplicationId);
        return errorAnswer(QStringLiteral("Job Crush could not save the note."));
    }
    return { QStringLiteral("Note added to #%1.").arg(card.campaign.jobApplicationId), false };
}

// Sorts the open cards into three lists: saved but not applied to, applied
// long ago with no news, and at the interview stage. Offers and closed jobs
// are left out.
AiAppAnswer AiAppRequests::whatsWaiting(const QDate &today) const
{
    QStringList notAppliedYet;
    QStringList followUp;
    QStringList interviewing;

    for (const TargetedJob &card : board.everyTargetedJob()) {
        switch (card.campaign.pipelineStage) {
        case PipelineStage::Saved:
            notAppliedYet.append(lineForCard(card, activeJobs));
            break;
        case PipelineStage::Applied:
            if (card.campaign.appliedTimestamp.isValid()
                    && card.campaign.appliedTimestamp.date().daysTo(today) > daysBeforeFollowUp) {
                followUp.append(lineForCard(card, activeJobs));
            }
            break;
        case PipelineStage::Interview:
            interviewing.append(lineForCard(card, activeJobs));
            break;
        case PipelineStage::Offer:
        case PipelineStage::Closed:
            break;
        }
    }

    if (notAppliedYet.isEmpty() && followUp.isEmpty() && interviewing.isEmpty()) {
        return { QStringLiteral("Nothing is waiting on the user right now."), false };
    }

    QStringList sections;
    if (!notAppliedYet.isEmpty()) {
        sections.append(QStringLiteral("Saved but not applied to yet:\n")
                        + notAppliedYet.join(QLatin1Char('\n')));
    }
    if (!followUp.isEmpty()) {
        sections.append(QStringLiteral("Applied over two weeks ago with no news. Time to "
                                       "follow up:\n")
                        + followUp.join(QLatin1Char('\n')));
    }
    if (!interviewing.isEmpty()) {
        sections.append(QStringLiteral("At the interview stage:\n")
                        + interviewing.join(QLatin1Char('\n')));
    }
    return { sections.join(QStringLiteral("\n\n")), false };
}
