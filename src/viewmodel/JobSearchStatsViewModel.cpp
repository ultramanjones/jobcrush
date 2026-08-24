#include "JobSearchStatsViewModel.h"

#include <QVariantMap>

#include "../modelview/stats/JobSearchStatistics.h"
#include "../modelview/tasks/StagingWorkbench.h"

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

int JobSearchStatsViewModel::busiestWeekCount() const
{
    int busiest = 0;
    for (const JobSearchStatistics::WeekOfEffort &week : jobSearchStatistics.weeksOfEffort()) {
        busiest = qMax(busiest, qMax(week.crushedCount, week.appliedCount));
    }
    return busiest;
}

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
    const int rate = jobSearchStatistics.replyRatePercent();
    if (rate >= 0) {
        return QStringLiteral("%1%").arg(rate);
    }
    // Too few applications for a percent. Show the count instead. With four
    // applications, one more reply moves the percent 25 points.
    return QString::number(jobSearchStatistics.totalReachedInterview());
}

QString JobSearchStatsViewModel::replyRateExplanationText() const
{
    const int applied = jobSearchStatistics.totalApplied();
    const int interviews = jobSearchStatistics.totalReachedInterview();

    if (applied == 0) {
        return QStringLiteral("nothing sent yet");
    }
    if (jobSearchStatistics.replyRatePercent() < 0) {
        return interviews == 1
            ? QStringLiteral("1 of %1 went somewhere — too few to call it a rate")
                  .arg(applied)
            : QStringLiteral("%1 of %2 went somewhere — too few to call it a rate")
                  .arg(interviews).arg(applied);
    }
    return QStringLiteral("%1 of %2 reached an interview").arg(interviews).arg(applied);
}

QString JobSearchStatsViewModel::timeToApplyText() const
{
    const int days = jobSearchStatistics.averageDaysToApply();
    if (days < 0) {
        return QStringLiteral("—");
    }
    if (days == 0) {
        return QStringLiteral("same day");
    }
    return days == 1 ? QStringLiteral("1 day") : QStringLiteral("%1 days").arg(days);
}

QString JobSearchStatsViewModel::streakText() const
{
    const int days = jobSearchStatistics.currentActivityStreakDays();
    if (days == 0) {
        return QStringLiteral("—");
    }
    return days == 1 ? QStringLiteral("1 day") : QStringLiteral("%1 days").arg(days);
}

QString JobSearchStatsViewModel::streakExplanationText() const
{
    const int days = jobSearchStatistics.currentActivityStreakDays();
    if (days == 0) {
        // Not a scolding. A day off during a job search is fine.
        return QStringLiteral("nothing yet today — that's allowed");
    }
    return QStringLiteral("days in a row you did something");
}
