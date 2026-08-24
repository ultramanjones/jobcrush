#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class JobSearchStatistics;
class StagingWorkbench;

// JobSearchStatsViewModel
//
// Formats the numbers for the Stats page, including the text shown instead of
// a percent when there is not enough data for one.
//
// The empty-state text is written here rather than in the view. A new user
// sees the empty dashboard first, and "0 / 0 / 0%" reads like failure at
// something they have not started.
class JobSearchStatsViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool hasAnythingToShow READ hasAnythingToShow NOTIFY statisticsChanged)
    Q_PROPERTY(QString emptyStateText READ emptyStateText NOTIFY statisticsChanged)

    Q_PROPERTY(QVariantList funnelSteps READ funnelSteps NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList weeksOfEffort READ weeksOfEffort NOTIFY statisticsChanged)
    Q_PROPERTY(QVariantList daysOfEffort READ daysOfEffort NOTIFY statisticsChanged)
    Q_PROPERTY(int busiestWeekCount READ busiestWeekCount NOTIFY statisticsChanged)
    Q_PROPERTY(int busiestDayCount READ busiestDayCount NOTIFY statisticsChanged)

    Q_PROPERTY(QString onTheBoardText READ onTheBoardText NOTIFY statisticsChanged)
    Q_PROPERTY(QString appliedText READ appliedText NOTIFY statisticsChanged)
    Q_PROPERTY(QString replyRateText READ replyRateText NOTIFY statisticsChanged)
    Q_PROPERTY(QString replyRateExplanationText READ replyRateExplanationText
                   NOTIFY statisticsChanged)
    Q_PROPERTY(QString timeToApplyText READ timeToApplyText NOTIFY statisticsChanged)
    Q_PROPERTY(QString streakText READ streakText NOTIFY statisticsChanged)
    Q_PROPERTY(QString streakExplanationText READ streakExplanationText NOTIFY statisticsChanged)

public:
    JobSearchStatsViewModel(JobSearchStatistics &statistics,
                            StagingWorkbench &workbench,
                            QObject *parent = nullptr);

    bool hasAnythingToShow() const;
    QString emptyStateText() const;

    QVariantList funnelSteps() const;
    QVariantList weeksOfEffort() const;
    QVariantList daysOfEffort() const;
    int busiestWeekCount() const;
    int busiestDayCount() const;

    QString onTheBoardText() const;
    QString appliedText() const;
    QString replyRateText() const;
    QString replyRateExplanationText() const;
    QString timeToApplyText() const;
    QString streakText() const;
    QString streakExplanationText() const;

    // Recounts on demand. The Stats page calls this when it opens.
    Q_INVOKABLE void refresh();

signals:
    void statisticsChanged();

private:
    JobSearchStatistics &jobSearchStatistics;
};
