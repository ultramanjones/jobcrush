#include "JobSearchStatsViewModel.h"

#include <QVariantMap>

#include "../logic/stats/JobSearchStatistics.h"
#include "../logic/tasks/StagingWorkbench.h"

JobSearchStatsViewModel::JobSearchStatsViewModel(JobSearchStatistics &statistics,
                                                 StagingWorkbench &workbench,
                                                 QObject *parent)
    : QObject(parent)
    , jobSearchStatistics(statistics)
{
    connect(&statistics, &JobSearchStatistics::statisticsChanged, this,
            [this]() { emit statisticsChanged(); });

    // Writing a draft counts as activity, so the heatmap needs to know.
    connect(&workbench, &StagingWorkbench::packetChanged, this,
            [this](qint64) { jobSearchStatistics.recount(); });
}

void JobSearchStatsViewModel::refresh()
{
    jobSearchStatistics.recount();
}

bool JobSearchStatsViewModel::hasAnythingToShow() const
{
    return jobSearchStatistics.totalOnTheBoard() > 0;
}

QString JobSearchStatsViewModel::emptyStateText() const
{
    return QStringLiteral(
        "Nothing to count yet.\n\nCrush a job from Discoveries and this page will "
        "start tracking: how many jobs you have going, how many turned into an "
        "interview, and how long you take to send an application after you decide "
        "to.\n\nReading postings and writing drafts count too. Those days are not "
        "blank days.");
}

QVariantList JobSearchStatsViewModel::funnelSteps() const
{
    QVariantList steps;
    const QList<JobSearchStatistics::FunnelStep> countedFunnel = jobSearchStatistics.funnel();

    int widestCount = 0;
    for (const JobSearchStatistics::FunnelStep &step : countedFunnel) {
        widestCount = qMax(widestCount, step.count);
    }

    for (const JobSearchStatistics::FunnelStep &step : countedFunnel) {
        QVariantMap stepMap;
        stepMap[QStringLiteral("displayName")] = step.displayName;
        stepMap[QStringLiteral("count")] = step.count;
        // Share of the widest bar, so the funnel still has a shape when all
        // the numbers are small.
        stepMap[QStringLiteral("shareOfWidest")] =
            widestCount > 0 ? static_cast<double>(step.count) / widestCount : 0.0;
        stepMap[QStringLiteral("stageName")] =
            pipelineStageToStorageText(step.pipelineStage);
        steps.append(stepMap);
    }
    return steps;
}

QVariantList JobSearchStatsViewModel::weeksOfEffort() const
{
    QVariantList weeks;
    for (const JobSearchStatistics::WeekOfEffort &week : jobSearchStatistics.weeksOfEffort()) {
        QVariantMap weekMap;
        weekMap[QStringLiteral("weekStartDate")] = week.weekStartDate;
        weekMap[QStringLiteral("weekLabel")] =
            week.weekStartDate.toString(QStringLiteral("d MMM"));
        weekMap[QStringLiteral("crushedCount")] = week.crushedCount;
        weekMap[QStringLiteral("appliedCount")] = week.appliedCount;
        weekMap[QStringLiteral("repliedCount")] = week.repliedCount;
        weeks.append(weekMap);
    }
    return weeks;
}

QVariantList JobSearchStatsViewModel::daysOfEffort() const
{
    QVariantList days;
    for (const JobSearchStatistics::DayOfEffort &day : jobSearchStatistics.daysOfEffort()) {
        QVariantMap dayMap;
        dayMap[QStringLiteral("date")] = day.date;
        dayMap[QStringLiteral("dayOfWeek")] = day.date.dayOfWeek();   // 1 = Monday
        dayMap[QStringLiteral("activityCount")] = day.activityCount;
        dayMap[QStringLiteral("dateLabel")] =
            day.date.toString(QStringLiteral("ddd d MMM yyyy"));
        days.append(dayMap);
    }
    return days;
}

// The tallest bar in the weekly chart. Each week draws a crushed bar and an
// applied bar, so the larger of the two counts. The view scales every bar
// against this number.
int JobSearchStatsViewModel::busiestWeekCount() const
{
    int busiest = 0;
    for (const JobSearchStatistics::WeekOfEffort &week : jobSearchStatistics.weeksOfEffort()) {
        busiest = qMax(busiest, qMax(week.crushedCount, week.appliedCount));
    }
    return busiest;
}

// The busiest day in the heatmap. The view shades every day against it.
int JobSearchStatsViewModel::busiestDayCount() const
{
    int busiest = 0;
    for (const JobSearchStatistics::DayOfEffort &day : jobSearchStatistics.daysOfEffort()) {
        busiest = qMax(busiest, day.activityCount);
    }
    return busiest;
}

QString JobSearchStatsViewModel::onTheBoardText() const
{
    return QString::number(jobSearchStatistics.totalOnTheBoard());
}

QString JobSearchStatsViewModel::appliedText() const
{
    return QString::number(jobSearchStatistics.totalApplied());
}

QString JobSearchStatsViewModel::replyRateText() const
{
    // replyRatePercent is -1 when there are too few applications for a
    // percent to mean anything. Then the count is shown instead. With four
    // applications, one more reply moves the percent 25 points.
    const int replyRatePercent = jobSearchStatistics.replyRatePercent();
    if (replyRatePercent >= 0) {
        return QStringLiteral("%1%").arg(replyRatePercent);
    }
    return QString::number(jobSearchStatistics.totalReachedInterview());
}

QString JobSearchStatsViewModel::replyRateExplanationText() const
{
    const int appliedCount = jobSearchStatistics.totalApplied();
    const int interviewCount = jobSearchStatistics.totalReachedInterview();

    if (appliedCount == 0) {
        return QStringLiteral("nothing sent yet");
    }
    if (jobSearchStatistics.replyRatePercent() < 0) {
        return interviewCount == 1
            ? QStringLiteral("1 of %1 went somewhere — too few to call it a rate")
                  .arg(appliedCount)
            : QStringLiteral("%1 of %2 went somewhere — too few to call it a rate")
                  .arg(interviewCount).arg(appliedCount);
    }
    return QStringLiteral("%1 of %2 reached an interview")
        .arg(interviewCount).arg(appliedCount);
}

QString JobSearchStatsViewModel::timeToApplyText() const
{
    // averageDaysToApply is -1 when nothing has been sent yet.
    const int averageDays = jobSearchStatistics.averageDaysToApply();
    if (averageDays < 0) {
        return QStringLiteral("—");
    }
    if (averageDays == 0) {
        return QStringLiteral("same day");
    }
    return averageDays == 1 ? QStringLiteral("1 day")
                            : QStringLiteral("%1 days").arg(averageDays);
}

QString JobSearchStatsViewModel::streakText() const
{
    const int streakDays = jobSearchStatistics.currentActivityStreakDays();
    if (streakDays == 0) {
        return QStringLiteral("—");
    }
    return streakDays == 1 ? QStringLiteral("1 day")
                           : QStringLiteral("%1 days").arg(streakDays);
}

QString JobSearchStatsViewModel::streakExplanationText() const
{
    const int streakDays = jobSearchStatistics.currentActivityStreakDays();
    if (streakDays == 0) {
        // Not a scolding. A day off during a job search is fine.
        return QStringLiteral("nothing yet today — that's allowed");
    }
    return QStringLiteral("days in a row you did something");
}
