#include "DiscoveredJobListViewModel.h"

#include "../logic/pipelines/JobPipelines.h"

#include <QDate>
#include <QDesktopServices>
#include <QLocale>

#include "../logic/pipelines/TypedDateReader.h"

#include "../logic/jobscout/ClearanceDetector.h"
#include <QUrl>

#include "../logic/jobscout/JobPostingTextCleanup.h"
#include "../logic/jobscout/AtsBoardIdentity.h"
#include "../logic/jobscout/JobSourceDescriptor.h"

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

// The reserved tab name. Not a real source storage name, and it never can be
// one: every real source name is a plain word, and this one carries a hyphen
// and a prefix nothing else uses.
const QString manualAddTabName = QStringLiteral("jobcrush-manual-add");

// The reserved name for the "Outside your area" tab. Same rule as above.
const QString outsideSearchAreaTabName = QStringLiteral("jobcrush-outside-area");

// A posting's source is either one of the job SITES Job Crush sweeps, or one
// of the employer BOARDS it reads. Two separate rosters, and asking only the
// first one is how "ashby" reached the screen in lowercase, looking like a
// bug in a row the user had just added.
QString sourceDisplayNameFor(const QString &sourceStorageName)
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

    // Unknown to both. Show what is stored rather than nothing — a blank
    // where a source belongs looks like a fault.
    return sourceStorageName;
}

} // namespace

DiscoveredJobListViewModel::DiscoveredJobListViewModel(JobScout &jobScout,
                                                       JobPipelines &pipelines,
                                                       QObject *parent)
    : QAbstractListModel(parent)
    , discoveryJobScout(jobScout)
    , board(pipelines)
{
    connect(&discoveryJobScout, &JobScout::discoveriesChanged, this, [this]() {
        rebuildRowsFromJobScout();
    });

    connect(&discoveryJobScout, &JobScout::sweepProgressChanged, this, [this]() {
        emit sweepProgressChanged();
    });

    connect(&discoveryJobScout, &JobScout::pastedTextFormChanged,
            this, &DiscoveredJobListViewModel::pastedTextFormChanged);

    connect(&discoveryJobScout, &JobScout::leadStatusChanged, this, [this]() {
        emit leadStatusChanged();
    });

    // The CRUSH button on a row asks the board whether this job is already on
    // it. Removing that job over on Job Pipelines has to reach back here, or
    // the row goes on saying "on your board" and the button stays dead.
    connect(&board, &JobPipelines::boardChanged, this, [this]() {
        ++boardChangeCounter;
        emit boardChanged();
    });

    rebuildRowsFromJobScout();
}

void DiscoveredJobListViewModel::rebuildRowsFromJobScout()
{
    // The "Outside your area" tab only exists while a location filter does.
    // Clear the last place in Settings while it is showing and the tab
    // disappears. Leaving it selected would show an empty list with no tab
    // to explain it, so the page goes back to Top Prospects.
    if (storedActiveTabSourceName == outsideSearchAreaTabName
            && !discoveryJobScout.searchAreaIsNarrowed()) {
        storedActiveTabSourceName.clear();
        emit activeTabSourceNameChanged();
    }

    // The lists are small and rebuilt only when something genuinely changed
    // (a sweep finished, a tab switched, the profile was edited), so a full
    // reset keeps this translation layer honest and simple.
    beginResetModel();
    if (storedActiveTabSourceName == manualAddTabName) {
        // No search-area scope. These are the jobs the user asked for by
        // name, and the location filter has no business hiding one.
        displayedJobPostings = discoveryJobScout.handAddedJobPostings();
    } else if (storedActiveTabSourceName == outsideSearchAreaTabName) {
        // Every held-back job from every site, ranked the same way as Top
        // Prospects, so the best of them is still at the top.
        displayedJobPostings =
            discoveryJobScout.rankedTopProspects(SearchAreaScope::OutsideSearchArea);
    } else if (storedActiveTabSourceName.isEmpty()) {
        displayedJobPostings =
            discoveryJobScout.rankedTopProspects(SearchAreaScope::InsideSearchArea);
    } else {
        displayedJobPostings = discoveryJobScout.scoredJobPostingsFromSource(
            storedActiveTabSourceName, SearchAreaScope::InsideSearchArea);
    }
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
    case SourceDisplayNameRole: return sourceDisplayNameFor(jobPosting.postingSource);
    case PostingSourceDisplayNameRole:
        return jobPosting.postingSource.trimmed().isEmpty()
            ? QString()
            : sourceDisplayNameFor(jobPosting.postingSource);
    case WasAddedByHandRole:    return jobPosting.wasAddedByHand();
    case IsManualAllTheWayRole: return jobPosting.isManualAllTheWay();
    case JobPostingIdRole:      return jobPosting.jobPostingId;
    case PostingSeemsGoneRole:  return jobPosting.postingGoneSinceTimestamp.isValid();
    case AsksForClearanceRole:  return ClearanceDetector::asksForClearance(jobPosting);
    case PostingGoneTextRole:
        return jobPosting.postingGoneSinceTimestamp.isValid()
            ? QStringLiteral("Gone from the company's own board since %1. It may be "
                             "filled.")
                  .arg(QLocale().toString(jobPosting.postingGoneSinceTimestamp.date(),
                                          QStringLiteral("d MMM")))
            : QString();
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
        { PostingSourceDisplayNameRole,
                                 QByteArrayLiteral("postingSourceDisplayName") },
        { WasAddedByHandRole,    QByteArrayLiteral("wasAddedByHand") },
        { IsManualAllTheWayRole, QByteArrayLiteral("isManualAllTheWay") },
        { JobPostingIdRole,      QByteArrayLiteral("jobPostingId") },
        { PostingSeemsGoneRole,  QByteArrayLiteral("postingSeemsGone") },
        { PostingGoneTextRole,   QByteArrayLiteral("postingGoneText") },
        { AsksForClearanceRole,  QByteArrayLiteral("asksForClearance") },
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

QString DiscoveredJobListViewModel::manualAddTabSourceName() const
{
    return manualAddTabName;
}

bool DiscoveredJobListViewModel::showingManualAdd() const
{
    return storedActiveTabSourceName == manualAddTabName;
}

bool DiscoveredJobListViewModel::hasSomewhereToJumpTo() const
{
    return discoveryJobScout.mostRecentlyHandAddedJobPostingId() != 0;
}

int DiscoveredJobListViewModel::rowOfMostRecentlyHandAddedJob() const
{
    const qint64 wantedJobPostingId =
        discoveryJobScout.mostRecentlyHandAddedJobPostingId();
    if (wantedJobPostingId == 0) {
        return -1;
    }
    for (int rowIndex = 0; rowIndex < displayedJobPostings.count(); ++rowIndex) {
        if (displayedJobPostings.at(rowIndex).jobPosting.jobPostingId
                == wantedJobPostingId) {
            return rowIndex;
        }
    }
    return -1;
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

QString DiscoveredJobListViewModel::lastSweepNoticeText() const
{
    return discoveryJobScout.lastSweepNoticeText();
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
    return storedActiveTabSourceName == outsideSearchAreaTabName;
}

QString DiscoveredJobListViewModel::outsideSearchAreaTabSourceName() const
{
    return outsideSearchAreaTabName;
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

int DiscoveredJobListViewModel::boardRevision() const
{
    return boardChangeCounter;
}

QString DiscoveredJobListViewModel::crushJobPostingAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= displayedJobPostings.count()) {
        return QStringLiteral("That job isn't on this list any more — run a sweep and "
                              "try again.");
    }

    QString reasonText;
    board.crushJobPosting(displayedJobPostings.at(rowIndex).jobPosting.jobPostingId,
                          reasonText);

    // Either outcome is worth saying out loud. "Already on your board" is not
    // a failure — it is the answer to the question the user just asked.
    ++boardChangeCounter;
    emit boardChanged();
    return reasonText;
}

bool DiscoveredJobListViewModel::jobPostingAtIsOnTheBoard(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= displayedJobPostings.count()) {
        return false;
    }
    return board.jobPostingIsOnTheBoard(
        displayedJobPostings.at(rowIndex).jobPosting.jobPostingId);
}

bool DiscoveredJobListViewModel::leadIsBeingResolved() const
{
    return discoveryJobScout.leadIsBeingResolved();
}

QString DiscoveredJobListViewModel::leadStatusText() const
{
    return discoveryJobScout.leadStatusText();
}

void DiscoveredJobListViewModel::addJobFromLink(const QString &pastedLink)
{
    discoveryJobScout.addJobFromLink(pastedLink);
}

void DiscoveredJobListViewModel::addJobFromCompanyAndTitle(const QString &companyName,
                                                           const QString &positionTitle)
{
    discoveryJobScout.addJobFromCompanyAndTitle(companyName, positionTitle);
}

bool DiscoveredJobListViewModel::pastedTextIsBeingRead() const
{
    return discoveryJobScout.pastedTextIsBeingRead();
}

QString DiscoveredJobListViewModel::pastedTextStatusText() const
{
    return discoveryJobScout.pastedTextStatusText();
}

bool DiscoveredJobListViewModel::aBrainCanReadPastedText() const
{
    return discoveryJobScout.aBrainCanReadPastedText();
}

QVariantMap DiscoveredJobListViewModel::filledForm() const
{
    const JobPosting form = discoveryJobScout.filledFormFromPastedText();
    const bool hasForm = !form.companyName.isEmpty() || !form.positionTitle.isEmpty()
                         || !form.fullDescriptionText.isEmpty();
    QVariantMap formFields;
    formFields.insert(QStringLiteral("hasForm"), hasForm);
    formFields.insert(QStringLiteral("companyName"), form.companyName);
    formFields.insert(QStringLiteral("positionTitle"), form.positionTitle);
    formFields.insert(QStringLiteral("sourceUrl"), form.sourceUrl);
    formFields.insert(QStringLiteral("locationText"), form.locationText);
    formFields.insert(QStringLiteral("salaryText"), form.salaryText);
    formFields.insert(QStringLiteral("postedText"),
                      form.postedTimestamp.isValid()
                          ? TypedDateReader::editableDateTextFor(form.postedTimestamp.date())
                          : QString());
    formFields.insert(QStringLiteral("isRemoteRole"), form.isRemoteRole);
    formFields.insert(QStringLiteral("descriptionText"), form.fullDescriptionText);
    return formFields;
}

void DiscoveredJobListViewModel::readPastedText(const QString &pastedText)
{
    discoveryJobScout.readPastedText(pastedText);
}

void DiscoveredJobListViewModel::clearFilledForm()
{
    discoveryJobScout.clearFilledForm();
}

void DiscoveredJobListViewModel::addJobFromFilledForm(const QString &companyName,
                                                      const QString &positionTitle,
                                                      const QString &sourceUrl,
                                                      const QString &locationText,
                                                      const QString &salaryText,
                                                      const QString &postedText,
                                                      bool isRemoteRole)
{
    JobPosting formFields = discoveryJobScout.filledFormFromPastedText();
    formFields.companyName = companyName;
    formFields.positionTitle = positionTitle;
    formFields.sourceUrl = sourceUrl;
    formFields.locationText = locationText;
    formFields.salaryText = salaryText;
    formFields.isRemoteRole = isRemoteRole;
    QDate postedDate;
    formFields.postedTimestamp =
        TypedDateReader::readTypedDate(postedText, QDate::currentDate(), QLocale(), postedDate)
            ? QDateTime(postedDate, QTime(12, 0))
            : QDateTime();
    discoveryJobScout.addJobFromFilledForm(formFields);
}
