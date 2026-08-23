#include "DiscoveredJobListViewModel.h"

#include <QDate>
#include <QDesktopServices>
#include <QLocale>
#include <QUrl>

#include "../modelview/jobscout/JobPostingTextCleanup.h"
#include "../modelview/jobscout/JobSourceDescriptor.h"

namespace {

// The section header a row sits under. Recent days get words, because "Today"
// is what a person actually thinks, and older ones get a date.
QString postedDayTextFor(const JobPosting &jobPosting)
{
    const QDateTime stampToShow = jobPosting.postedTimestamp.isValid()
        ? jobPosting.postedTimestamp
        : jobPosting.discoveredTimestamp;
    if (!stampToShow.isValid()) {
        return QStringLiteral("No date given");
    }

    const QDate postedDate = stampToShow.date();
    const QDate todayDate = QDate::currentDate();

    if (postedDate == todayDate) {
        return QStringLiteral("Today");
    }
    if (postedDate == todayDate.addDays(-1)) {
        return QStringLiteral("Yesterday");
    }
    return QLocale().toString(postedDate, QStringLiteral("MMMM d"));
}

QString sourceDisplayNameFor(const QString &sourceStorageName)
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    return descriptorFound ? descriptor.displayName : sourceStorageName;
}

} // namespace

DiscoveredJobListViewModel::DiscoveredJobListViewModel(JobScout &jobScout, QObject *parent)
    : QAbstractListModel(parent)
    , discoveryJobScout(jobScout)
{
    connect(&discoveryJobScout, &JobScout::discoveriesChanged, this, [this]() {
        rebuildRowsFromJobScout();
    });

    connect(&discoveryJobScout, &JobScout::sweepProgressChanged, this, [this]() {
        emit sweepProgressChanged();
    });

    rebuildRowsFromJobScout();
}

void DiscoveredJobListViewModel::rebuildRowsFromJobScout()
{
    // The lists are small and rebuilt only when something genuinely changed
    // (a sweep finished, a tab switched, the profile was edited), so a full
    // reset keeps this translation layer honest and simple.
    const SearchAreaScope searchAreaScope = storedShowingOutsideSearchArea
        ? SearchAreaScope::OutsideSearchArea
        : SearchAreaScope::InsideSearchArea;

    beginResetModel();
    displayedJobPostings = storedActiveTabSourceName.isEmpty()
        ? discoveryJobScout.rankedTopProspects(searchAreaScope)
        : discoveryJobScout.scoredJobPostingsFromSource(storedActiveTabSourceName,
                                                        searchAreaScope);
    endResetModel();

    emit discoveredJobsChanged();
}

int DiscoveredJobListViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0; // flat list, no children
    }
    return displayedJobPostings.count();
}

QVariant DiscoveredJobListViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() >= displayedJobPostings.count()) {
        return QVariant();
    }

    const ScoredJobPosting &scoredJobPosting = displayedJobPostings.at(modelIndex.row());
    const JobPosting &jobPosting = scoredJobPosting.jobPosting;

    switch (role) {
    case PositionTitleRole:     return jobPosting.positionTitle;
    case CompanyNameRole:       return jobPosting.companyName;
    case LocationTextRole:      return jobPosting.locationText;
    case SalaryTextRole:        return jobPosting.salaryText;
    case SummaryLineRole:       return oneLineSummaryFrom(jobPosting.fullDescriptionText);
    case SourceDisplayNameRole: return sourceDisplayNameFor(jobPosting.discoverySource);
    case PostedDayTextRole:     return postedDayTextFor(jobPosting);
    case MatchScoreRole:        return scoredJobPosting.matchResult.matchScoreOutOfOneHundred;
    case MatchReasonsTextRole:
        return scoredJobPosting.matchResult.matchReasons.join(QStringLiteral(" · "));
    case IsRemoteRoleRole:      return jobPosting.isRemoteRole;
    }
    return QVariant();
}

QHash<int, QByteArray> DiscoveredJobListViewModel::roleNames() const
{
    return {
        { PositionTitleRole,     QByteArrayLiteral("positionTitle") },
        { CompanyNameRole,       QByteArrayLiteral("companyName") },
        { LocationTextRole,      QByteArrayLiteral("locationText") },
        { SalaryTextRole,        QByteArrayLiteral("salaryText") },
        { SummaryLineRole,       QByteArrayLiteral("summaryLine") },
        { SourceDisplayNameRole, QByteArrayLiteral("sourceDisplayName") },
        { PostedDayTextRole,     QByteArrayLiteral("postedDayText") },
        { MatchScoreRole,        QByteArrayLiteral("matchScore") },
        { MatchReasonsTextRole,  QByteArrayLiteral("matchReasonsText") },
        { IsRemoteRoleRole,      QByteArrayLiteral("isRemoteRole") },
    };
}

QString DiscoveredJobListViewModel::activeTabSourceName() const
{
    return storedActiveTabSourceName;
}

void DiscoveredJobListViewModel::setActiveTabSourceName(const QString &sourceStorageName)
{
    if (sourceStorageName == storedActiveTabSourceName) {
        return;
    }
    storedActiveTabSourceName = sourceStorageName;
    emit activeTabSourceNameChanged();
    rebuildRowsFromJobScout();
}

int DiscoveredJobListViewModel::rowCountForProperty() const
{
    return displayedJobPostings.count();
}

bool DiscoveredJobListViewModel::sweepIsRunning() const
{
    return discoveryJobScout.sweepIsRunning();
}

QString DiscoveredJobListViewModel::sweepProgressText() const
{
    return discoveryJobScout.sweepProgressText();
}

QString DiscoveredJobListViewModel::lastSweepSummaryText() const
{
    return discoveryJobScout.lastSweepSummaryText();
}

QString DiscoveredJobListViewModel::lastSweepTroubleText() const
{
    return discoveryJobScout.lastSweepTroubleText();
}

bool DiscoveredJobListViewModel::canRankProspects() const
{
    return discoveryJobScout.searchProfileCanRank();
}

int DiscoveredJobListViewModel::jobCountOutsideSearchArea() const
{
    return discoveryJobScout.jobPostingCountOutsideSearchArea();
}

bool DiscoveredJobListViewModel::searchAreaIsNarrowed() const
{
    return discoveryJobScout.searchAreaIsNarrowed();
}

bool DiscoveredJobListViewModel::showingOutsideSearchArea() const
{
    return storedShowingOutsideSearchArea;
}

void DiscoveredJobListViewModel::setShowingOutsideSearchArea(bool showOutside)
{
    if (showOutside == storedShowingOutsideSearchArea) {
        return;
    }
    storedShowingOutsideSearchArea = showOutside;
    emit showingOutsideSearchAreaChanged();
    rebuildRowsFromJobScout();
}

void DiscoveredJobListViewModel::startSweep()
{
    discoveryJobScout.startSweep();
}

void DiscoveredJobListViewModel::openJobPostingInBrowser(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= displayedJobPostings.count()) {
        return;
    }
    const QString sourceUrl = displayedJobPostings.at(rowIndex).jobPosting.sourceUrl;
    if (sourceUrl.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl(sourceUrl));
}
