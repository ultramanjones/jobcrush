#pragma once

#include <QDate>
#include <QList>
#include <QObject>
#include <QString>

#include "../../model/PipelineStage.h"

class JobApplicationRepository;
class JobPostingRepository;
class JobPipelines;
class StagedDocumentRepository;

// JobSearchStatistics
//
// Counts the numbers for the Stats page. A ModelView class. It counts. It does
// not draw, and it knows nothing about charts.
//
// One rule shapes this file: a job search produces small numbers, and a
// percentage from a small number is misleading. Three applications and one
// interview is not a 33% interview rate. It is three applications. So every
// rate here comes with the counts behind it, and rates with too little data
// are not shown at all.
class JobSearchStatistics : public QObject {
    Q_OBJECT
public:
    // One level of the funnel, and how many jobs reached it.
    struct FunnelStep {
        PipelineStage pipelineStage = PipelineStage::Saved;
        QString displayName;
        int count = 0;
    };

    // One week of activity, for the week-by-week chart.
    struct WeekOfEffort {
        QDate weekStartDate;
        int crushedCount = 0;      // jobs put on the board
        int appliedCount = 0;      // applications sent
        int repliedCount = 0;      // that later reached interview or offer
    };

    // One day, for the heatmap. All work counts, including reading postings
    // and writing drafts. A heatmap that only marks days you applied would
    // show nothing for the days you did the hard part.
    struct DayOfEffort {
        QDate date;
        int activityCount = 0;
    };

    JobSearchStatistics(JobPipelines &board,
                        StagedDocumentRepository &packetRepository,
                        QObject *parent = nullptr);

    // Recounts everything. Cheap. This is hundreds of rows, not millions.
    void recount();

    QList<FunnelStep> funnel() const;

    // The last 26 weeks, oldest first, including empty weeks. An empty week
    // is real and should be shown.
    QList<WeekOfEffort> weeksOfEffort() const;

    // The last 26 weeks of days, oldest first, for the heatmap.
    QList<DayOfEffort> daysOfEffort() const;

    int totalOnTheBoard() const;
    int totalApplied() const;
    int totalReachedInterview() const;
    int totalOffers() const;
    int totalClosed() const;

    // Applications that reached an interview, as a percent of applications
    // sent. Returns -1 when there are too few applications to show a percent.
    // See the note at the top of this file.
    int replyRatePercent() const;

    // The fewest applications needed before a percent is shown. Below this,
    // the screen shows the count instead.
    static int fewestApplicationsWorthARate();

    // Average days from crushing a job to sending the application. -1 when
    // nothing has been sent yet.
    int averageDaysToApply() const;

    // How many days in a row have had activity, counting back from today.
    // Zero when the streak is broken.
    int currentActivityStreakDays() const;

signals:
    void statisticsChanged();

private:
    JobPipelines &jobPipelines;
    StagedDocumentRepository &stagedDocumentRepository;

    QList<FunnelStep> countedFunnel;
    QList<WeekOfEffort> countedWeeks;
    QList<DayOfEffort> countedDays;

    int countOnTheBoard = 0;
    int countApplied = 0;
    int countInterview = 0;
    int countOffer = 0;
    int countClosed = 0;
    int averageDaysBetweenCrushAndApply = -1;
    int streakDays = 0;
};
