#include "JobSearchStatistics.h"

#include <QDateTime>
#include <QHash>

#include "../../model/StagedDocument.h"
#include "../../model/StagedDocumentRepository.h"
#include "../pipelines/JobPipelines.h"

namespace {

// How far back the two time charts look. Six months. Long enough to show a
// pattern, short enough that a search started last week is not one bump on a
// flat line.
constexpr int weeksShown = 26;
constexpr int daysShown = 182;

// Below this many applications, a percent is misleading. Show the count.
constexpr int fewestApplicationsForARate = 5;

QDate startOfWeekFor(const QDate &date)
{
    // Weeks start on Monday, which is how people talk about their own week.
    return date.addDays(-(date.dayOfWeek() - 1));
}

QString displayNameFor(PipelineStage pipelineStage)
{
    switch (pipelineStage) {
    case PipelineStage::Saved:     return QStringLiteral("On the board");
    case PipelineStage::Applied:   return QStringLiteral("Applied");
    case PipelineStage::Interview: return QStringLiteral("Interview");
    case PipelineStage::Offer:     return QStringLiteral("Offer");
    case PipelineStage::Closed:    return QStringLiteral("Closed");
    }
    return QString();
}

// The stages are LEVELS. Each level includes the ones before it. A job at the
// Interview level was applied to, whatever column its card is in today.
//
// This matters because a card sits in only one column at a time. Counting
// cards per column gives you numbers like "Applied 2, Interview 6", which
// would mean 6 jobs got interviews without applying. Counting levels gives
// "Applied 8, Interview 6", which is the real picture.
//
// Yes, someone could theoretically get an interview without applying. It is
// rare enough to ignore.
bool reachedAtLeast(PipelineStage stageOfCampaign, PipelineStage stageAsked)
{
    auto depthOf = [](PipelineStage stage) {
        switch (stage) {
        case PipelineStage::Saved:     return 0;
        case PipelineStage::Applied:   return 1;
        case PipelineStage::Interview: return 2;
        case PipelineStage::Offer:     return 3;
        case PipelineStage::Closed:    return 0; // see below
        }
        return 0;
    };
    if (stageOfCampaign == PipelineStage::Closed) {
        // Closed says where a campaign ended, not how far it got. The
        // applied date is the real evidence, and the caller checks it.
        return stageAsked == PipelineStage::Saved;
    }
    return depthOf(stageOfCampaign) >= depthOf(stageAsked);
}

} // namespace

JobSearchStatistics::JobSearchStatistics(JobPipelines &board,
                                         StagedDocumentRepository &packetRepository,
                                         QObject *parent)
    : QObject(parent)
    , jobPipelines(board)
    , stagedDocumentRepository(packetRepository)
{
    connect(&board, &JobPipelines::boardChanged, this, [this]() { recount(); });
    recount();
}

int JobSearchStatistics::fewestApplicationsWorthARate()
{
    return fewestApplicationsForARate;
}

void JobSearchStatistics::recount()
{
    const QList<TargetedJob> everyJob = jobPipelines.everyTargetedJob();
    const QDate today = QDate::currentDate();

    countOnTheBoard = everyJob.count();
    countApplied = 0;
    countInterview = 0;
    countOffer = 0;
    countClosed = 0;

    QHash<QDate, int> activityByDay;
    QHash<QDate, WeekOfEffort> effortByWeek;

    qint64 totalDaysBetweenCrushAndApply = 0;
    int campaignsWithBothDates = 0;

    for (const TargetedJob &targetedJob : everyJob) {
        const JobApplication &campaign = targetedJob.campaign;

        // Once an application is sent it stays counted as sent, whatever
        // column the card is in now. That includes Closed, where most
        // applications end up.
        const bool wasApplied = campaign.appliedTimestamp.isValid()
            || reachedAtLeast(campaign.pipelineStage, PipelineStage::Applied);
        if (wasApplied) {
            ++countApplied;
        }
        if (reachedAtLeast(campaign.pipelineStage, PipelineStage::Interview)) {
            ++countInterview;
        }
        if (reachedAtLeast(campaign.pipelineStage, PipelineStage::Offer)) {
            ++countOffer;
        }
        if (campaign.pipelineStage == PipelineStage::Closed) {
            ++countClosed;
        }

        if (campaign.targetedTimestamp.isValid()) {
            const QDate crushedOn = campaign.targetedTimestamp.date();
            activityByDay[crushedOn] += 1;

            WeekOfEffort &week = effortByWeek[startOfWeekFor(crushedOn)];
            week.weekStartDate = startOfWeekFor(crushedOn);
            week.crushedCount += 1;
        }

        if (campaign.appliedTimestamp.isValid()) {
            const QDate appliedOn = campaign.appliedTimestamp.date();
            activityByDay[appliedOn] += 1;

            WeekOfEffort &week = effortByWeek[startOfWeekFor(appliedOn)];
            week.weekStartDate = startOfWeekFor(appliedOn);
            week.appliedCount += 1;

            if (reachedAtLeast(campaign.pipelineStage, PipelineStage::Interview)) {
                week.repliedCount += 1;
            }
            if (campaign.targetedTimestamp.isValid()) {
                totalDaysBetweenCrushAndApply +=
                    campaign.targetedTimestamp.date().daysTo(appliedOn);
                ++campaignsWithBothDates;
            }
        }
    }

    // Writing a draft counts as activity. A day spent on four cover letters
    // that were not sent is not a day off.
    for (const StagedDocument &piece : stagedDocumentRepository.loadEveryStagedDocument()) {
        if (piece.createdTimestamp.isValid()) {
            activityByDay[piece.createdTimestamp.date()] += 1;
        }
    }

    averageDaysBetweenCrushAndApply = campaignsWithBothDates > 0
        ? static_cast<int>(totalDaysBetweenCrushAndApply / campaignsWithBothDates)
        : -1;

    // --- the funnel ---
    countedFunnel.clear();
    countedFunnel.append({ PipelineStage::Saved, displayNameFor(PipelineStage::Saved),
                           countOnTheBoard });
    countedFunnel.append({ PipelineStage::Applied, displayNameFor(PipelineStage::Applied),
                           countApplied });
    countedFunnel.append({ PipelineStage::Interview, displayNameFor(PipelineStage::Interview),
                           countInterview });
    countedFunnel.append({ PipelineStage::Offer, displayNameFor(PipelineStage::Offer),
                           countOffer });

    // --- the weeks, including empty ones ---
    countedWeeks.clear();
    const QDate thisWeekStart = startOfWeekFor(today);
    for (int weekNumber = weeksShown - 1; weekNumber >= 0; --weekNumber) {
        const QDate weekStart = thisWeekStart.addDays(-7 * weekNumber);
        WeekOfEffort week = effortByWeek.value(weekStart);
        week.weekStartDate = weekStart;
        countedWeeks.append(week);
    }

    // --- the days ---
    //
    // STARTS ON A MONDAY, always. The heatmap draws these seven to a column,
    // so a run that begins on a Thursday puts Thursday in the top row and
    // every weekday one row out of place — a grid that looks right and says
    // something false about which days somebody works.
    countedDays.clear();
    const QDate firstDayShown = startOfWeekFor(today.addDays(-(daysShown - 1)));
    for (QDate day = firstDayShown; day <= today; day = day.addDays(1)) {
        countedDays.append({ day, activityByDay.value(day, 0) });
    }

    // --- the streak ---
    //
    // Counted back from today. If there is no activity today yet, start at
    // yesterday. Otherwise the streak would show as broken every morning
    // before the user has done anything.
    streakDays = 0;
    QDate walkingDate = activityByDay.value(today, 0) > 0 ? today : today.addDays(-1);
    while (activityByDay.value(walkingDate, 0) > 0) {
        ++streakDays;
        walkingDate = walkingDate.addDays(-1);
    }

    emit statisticsChanged();
}

QList<JobSearchStatistics::FunnelStep> JobSearchStatistics::funnel() const
{
    return countedFunnel;
}

QList<JobSearchStatistics::WeekOfEffort> JobSearchStatistics::weeksOfEffort() const
{
    return countedWeeks;
}

QList<JobSearchStatistics::DayOfEffort> JobSearchStatistics::daysOfEffort() const
{
    return countedDays;
}

int JobSearchStatistics::totalOnTheBoard() const   { return countOnTheBoard; }
int JobSearchStatistics::totalApplied() const      { return countApplied; }
int JobSearchStatistics::totalReachedInterview() const { return countInterview; }
int JobSearchStatistics::totalOffers() const       { return countOffer; }
int JobSearchStatistics::totalClosed() const       { return countClosed; }
int JobSearchStatistics::averageDaysToApply() const { return averageDaysBetweenCrushAndApply; }
int JobSearchStatistics::currentActivityStreakDays() const { return streakDays; }

int JobSearchStatistics::replyRatePercent() const
{
    if (countApplied < fewestApplicationsForARate) {
        return -1;
    }
    return (countInterview * 100) / countApplied;
}
