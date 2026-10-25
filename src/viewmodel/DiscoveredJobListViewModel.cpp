#include "DiscoveredJobListViewModel.h"

#include <QDate>
#include <QDebug>
#include <QDesktopServices>
#include <QLocale>
#include <QUrl>

#include "../logic/jobscout/AtsBoardIdentity.h"
#include "../logic/jobscout/ClearanceDetector.h"
#include "../logic/jobscout/JobPostingTextCleanup.h"
#include "../logic/jobscout/JobSourceDescriptor.h"
#include "../logic/pipelines/JobPipelines.h"
#include "../logic/pipelines/TypedDateReader.h"

namespace {

// A typed date is stored at this time of day. See addJobFromFilledForm.
const QTime timeOfDayForTypedDates(12, 0);

// The reserved tab names. Neither can ever match a real source's storage
// name, because every real one is a plain word with no hyphen.
const QString manualAddTabName = QStringLiteral("jobcrush-manual-add");
const QString outsideSearchAreaTabName = QStringLiteral("jobcrush-outside-area");

// The section header a row sits under. The last two days get words, because
// "Today" is easier to read than a date. Older days get a date.
QString postedDayTextFor(const JobPosting &jobPosting)
{
    const QDateTime timestampToShow = jobPosting.postedTimestamp.isValid()
        ? jobPosting.postedTimestamp
        : jobPosting.discoveredTimestamp;
    if (!timestampToShow.isValid()) {
        return QStringLiteral("No date given");
    }

    const QDate postedDate = timestampToShow.date();
    const QDate todayDate = QDate::currentDate();

    if (postedDate == todayDate) {
        return QStringLiteral("Today");
    }
    if (postedDate == todayDate.addDays(-1)) {
        return QStringLiteral("Yesterday");
    }
    return QLocale().toString(postedDate, QStringLiteral("MMMM d"));
}

// The name to show for where a posting came from. A source is either a job
// SITE that Job Crush sweeps, or an employer BOARD it reads. They are listed
// in two different places, so both are asked. Asking only the first once put
// "ashby" on screen in lowercase.
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

    // Unknown to both lists. Show the stored name, because a blank where the
    // source belongs looks like a fault.
    return sourceStorageName;
}

QString matchStrengthTextFor(int matchScoreOutOfOneHundred)
{
    switch (ProspectScorer::matchStrengthFor(matchScoreOutOfOneHundred)) {
    case ProspectMatchStrength::Strong: return QStringLiteral("strong");
    case ProspectMatchStrength::Fair:   return QStringLiteral("fair");
    case ProspectMatchStrength::Weak:   return QStringLiteral("weak");
    }
    return QStringLiteral("weak");
}

} // namespace

DiscoveredJobListViewModel::DiscoveredJobListViewModel(JobScout &jobScout,
                                                       JobPipelines &jobPipelines,
                                                       QObject *parent)
    : QAbstractListModel(parent)
    , discoveryJobScout(jobScout)
    , jobPipelines(jobPipelines)
{
    connect(&discoveryJobScout, &JobScout::discoveriesChanged,
            this, &DiscoveredJobListViewModel::requestRowsForTheActiveTab);
    connect(&discoveryJobScout, &JobScout::rowsReady,
            this, &DiscoveredJobListViewModel::whenRowsAreReady);
    connect(&discoveryJobScout, &JobScout::sweepProgressChanged,
            this, &DiscoveredJobListViewModel::sweepProgressChanged);
    connect(&discoveryJobScout, &JobScout::pastedTextFormChanged,
            this, &DiscoveredJobListViewModel::pastedTextFormChanged);
    connect(&discoveryJobScout, &JobScout::leadStatusChanged,
            this, &DiscoveredJobListViewModel::leadStatusChanged);

    // A job can be put on the board, or taken off it, from this page or from
    // Job Pipelines. Either way, every row's CRUSH button has to update.
    connect(&jobPipelines, &JobPipelines::boardChanged,
            this, &DiscoveredJobListViewModel::refreshIsOnTheBoardForEveryRow);

    requestRowsForTheActiveTab();
}

void DiscoveredJobListViewModel::requestRowsForTheActiveTab()
{
    // The "Outside your area" tab exists only while the user has named a
    // place. If they clear the last place in Settings while that tab is
    // showing, the tab goes away, so the page goes back to Top Prospects
    // instead of showing an empty list under no tab.
    if (storedActiveTabSourceName == outsideSearchAreaTabName
            && !discoveryJobScout.searchAreaIsNarrowed()) {
        storedActiveTabSourceName.clear();
        emit activeTabSourceNameChanged();
    }

    DiscoveryRowRequest rowRequest;
    if (storedActiveTabSourceName == manualAddTabName) {
        rowRequest.rowKind = DiscoveryRowRequest::RowKind::HandAdded;
    } else if (storedActiveTabSourceName == outsideSearchAreaTabName) {
        rowRequest.rowKind = DiscoveryRowRequest::RowKind::OutsideSearchArea;
    } else if (storedActiveTabSourceName.isEmpty()) {
        rowRequest.rowKind = DiscoveryRowRequest::RowKind::TopProspects;
    } else {
        rowRequest.rowKind = DiscoveryRowRequest::RowKind::OneSource;
        rowRequest.sourceStorageName = storedActiveTabSourceName;
    }
    newestRowRequestNumber = discoveryJobScout.requestRows(rowRequest);
}

void DiscoveredJobListViewModel::whenRowsAreReady(int requestNumber,
                                                  const DiscoveryRows &preparedRows)
{
    if (requestNumber != newestRowRequestNumber) {
        return;
    }

    // A full reset is simple, and fast enough with the scoring already done.
    beginResetModel();
    displayedJobPostings = preparedRows.scoredJobPostings;
    endResetModel();

    waitingForRows = false;
    storedJobCountOutsideSearchArea = preparedRows.jobCountOutsideSearchArea;
    emit discoveredJobsChanged();
}

void DiscoveredJobListViewModel::refreshIsOnTheBoardForEveryRow()
{
    if (displayedJobPostings.isEmpty()) {
        return;
    }
    emit dataChanged(index(0), index(displayedJobPostings.count() - 1), { IsOnTheBoardRole });
}

int DiscoveredJobListViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0; // a flat list, so no row has children
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
    const int matchScore = scoredJobPosting.matchResult.matchScoreOutOfOneHundred;

    switch (role) {
    case PositionTitleRole:     return jobPosting.positionTitle;
    case CompanyNameRole:       return jobPosting.companyName;
    case LocationTextRole:      return jobPosting.locationText;
    case SalaryTextRole:        return jobPosting.salaryText;
    case SummaryLineRole:       return oneLineSummaryFrom(jobPosting.fullDescriptionText);
    case SourceDisplayNameRole: return sourceDisplayNameFor(jobPosting.postingSource);
    case PostedDayTextRole:     return postedDayTextFor(jobPosting);
    case MatchScoreRole:        return matchScore;
    case MatchReasonsTextRole:
        return scoredJobPosting.matchResult.matchReasons.join(QStringLiteral(" · "));
    case MatchStrengthRole:     return matchStrengthTextFor(matchScore);
    case IsRemoteRoleRole:      return jobPosting.isRemoteRole;
    case PostingSourceDisplayNameRole:
        return jobPosting.postingSource.trimmed().isEmpty()
            ? QString()
            : sourceDisplayNameFor(jobPosting.postingSource);
    case WasAddedByHandRole:    return jobPosting.wasAddedByHand();
    case IsManualAllTheWayRole: return jobPosting.isManualAllTheWay();
    case JobPostingIdRole:      return jobPosting.jobPostingId;
    case PostingSeemsGoneRole:  return jobPosting.postingGoneSinceTimestamp.isValid();
    case PostingGoneTextRole:
        return jobPosting.postingGoneSinceTimestamp.isValid()
            ? QStringLiteral("Gone from the company's own board since %1. It may be "
                             "filled.")
                  .arg(QLocale().toString(jobPosting.postingGoneSinceTimestamp.date(),
                                          QStringLiteral("MMMM d")))
            : QString();
    case AsksForClearanceRole:  return ClearanceDetector::asksForClearance(jobPosting);
    case IsOnTheBoardRole:      return jobPipelines.jobPostingIsOnTheBoard(jobPosting.jobPostingId);
    }
    return QVariant();
}

QHash<int, QByteArray> DiscoveredJobListViewModel::roleNames() const
{
    return {
        { PositionTitleRole,            QByteArrayLiteral("positionTitle") },
        { CompanyNameRole,              QByteArrayLiteral("companyName") },
        { LocationTextRole,             QByteArrayLiteral("locationText") },
        { SalaryTextRole,               QByteArrayLiteral("salaryText") },
        { SummaryLineRole,              QByteArrayLiteral("summaryLine") },
        { SourceDisplayNameRole,        QByteArrayLiteral("sourceDisplayName") },
        { PostedDayTextRole,            QByteArrayLiteral("postedDayText") },
        { MatchScoreRole,               QByteArrayLiteral("matchScore") },
        { MatchReasonsTextRole,         QByteArrayLiteral("matchReasonsText") },
        { MatchStrengthRole,            QByteArrayLiteral("matchStrength") },
        { IsRemoteRoleRole,             QByteArrayLiteral("isRemoteRole") },
        { PostingSourceDisplayNameRole, QByteArrayLiteral("postingSourceDisplayName") },
        { WasAddedByHandRole,           QByteArrayLiteral("wasAddedByHand") },
        { IsManualAllTheWayRole,        QByteArrayLiteral("isManualAllTheWay") },
        { JobPostingIdRole,             QByteArrayLiteral("jobPostingId") },
        { PostingSeemsGoneRole,         QByteArrayLiteral("postingSeemsGone") },
        { PostingGoneTextRole,          QByteArrayLiteral("postingGoneText") },
        { AsksForClearanceRole,         QByteArrayLiteral("asksForClearance") },
        { IsOnTheBoardRole,             QByteArrayLiteral("isOnTheBoard") },
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

    // The old tab's rows go at once, so they never show under the new tab's
    // name while the new rows are being prepared.
    beginResetModel();
    displayedJobPostings.clear();
    endResetModel();
    waitingForRows = true;
    emit discoveredJobsChanged();

    requestRowsForTheActiveTab();
}

QString DiscoveredJobListViewModel::manualAddTabSourceName() const
{
    return manualAddTabName;
}

QString DiscoveredJobListViewModel::outsideSearchAreaTabSourceName() const
{
    return outsideSearchAreaTabName;
}

bool DiscoveredJobListViewModel::showingManualAdd() const
{
    return storedActiveTabSourceName == manualAddTabName;
}

bool DiscoveredJobListViewModel::showingOutsideSearchArea() const
{
    return storedActiveTabSourceName == outsideSearchAreaTabName;
}

int DiscoveredJobListViewModel::rowCountForProperty() const
{
    return displayedJobPostings.count();
}

bool DiscoveredJobListViewModel::canRankProspects() const
{
    return discoveryJobScout.searchProfileCanRank();
}

int DiscoveredJobListViewModel::jobCountOutsideSearchArea() const
{
    return storedJobCountOutsideSearchArea;
}

bool DiscoveredJobListViewModel::rowsAreLoading() const
{
    return waitingForRows;
}

bool DiscoveredJobListViewModel::searchAreaIsNarrowed() const
{
    return discoveryJobScout.searchAreaIsNarrowed();
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

bool DiscoveredJobListViewModel::leadIsBeingResolved() const
{
    return discoveryJobScout.leadIsBeingResolved();
}

QString DiscoveredJobListViewModel::leadStatusText() const
{
    return discoveryJobScout.leadStatusText();
}

bool DiscoveredJobListViewModel::hasSomewhereToJumpTo() const
{
    return discoveryJobScout.mostRecentlyHandAddedJobPostingId() != 0;
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
    const JobPosting pastedTextForm = discoveryJobScout.filledFormFromPastedText();
    const bool hasForm = !pastedTextForm.companyName.isEmpty()
                         || !pastedTextForm.positionTitle.isEmpty()
                         || !pastedTextForm.fullDescriptionText.isEmpty();
    QVariantMap formFields;
    formFields.insert(QStringLiteral("hasForm"), hasForm);
    formFields.insert(QStringLiteral("companyName"), pastedTextForm.companyName);
    formFields.insert(QStringLiteral("positionTitle"), pastedTextForm.positionTitle);
    formFields.insert(QStringLiteral("sourceUrl"), pastedTextForm.sourceUrl);
    formFields.insert(QStringLiteral("locationText"), pastedTextForm.locationText);
    formFields.insert(QStringLiteral("salaryText"), pastedTextForm.salaryText);
    formFields.insert(QStringLiteral("postedText"),
                      pastedTextForm.postedTimestamp.isValid()
                          ? TypedDateReader::editableDateTextFor(
                                pastedTextForm.postedTimestamp.date())
                          : QString());
    formFields.insert(QStringLiteral("isRemoteRole"), pastedTextForm.isRemoteRole);
    formFields.insert(QStringLiteral("descriptionText"), pastedTextForm.fullDescriptionText);
    return formFields;
}

void DiscoveredJobListViewModel::startSweep()
{
    discoveryJobScout.startSweep();
}

void DiscoveredJobListViewModel::openJobPostingInBrowser(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= displayedJobPostings.count()) {
        qWarning().noquote() << QStringLiteral(
            "DiscoveredJobListViewModel::openJobPostingInBrowser: row %1 is not on "
            "the list, which has %2 rows").arg(rowIndex).arg(displayedJobPostings.count());
        return;
    }
    const JobPosting &jobPosting = displayedJobPostings.at(rowIndex).jobPosting;
    if (jobPosting.sourceUrl.isEmpty()) {
        return; // a hand-added job with no link has nothing to open
    }

    // Only web links are opened. The link came from a job board, not from
    // the user, and a link like file:// or a program's own scheme could make
    // the computer do something other than show a web page.
    const QUrl postingUrl(jobPosting.sourceUrl);
    const QString urlScheme = postingUrl.scheme().toLower();
    if (urlScheme != QStringLiteral("https") && urlScheme != QStringLiteral("http")) {
        qWarning().noquote() << QStringLiteral(
            "DiscoveredJobListViewModel::openJobPostingInBrowser: refused to open "
            "\"%1\" for job posting %2, because it is not a web link")
                .arg(jobPosting.sourceUrl).arg(jobPosting.jobPostingId);
        return;
    }
    if (!QDesktopServices::openUrl(postingUrl)) {
        qWarning().noquote() << QStringLiteral(
            "DiscoveredJobListViewModel::openJobPostingInBrowser: could not open \"%1\" "
            "for job posting %2 — the system browser refused it")
                .arg(jobPosting.sourceUrl).arg(jobPosting.jobPostingId);
    }
}

QString DiscoveredJobListViewModel::crushJobPostingAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= displayedJobPostings.count()) {
        return QStringLiteral("That job isn't on this list any more. Run a sweep and "
                              "try again.");
    }

    // The sentence comes back whether the crush worked or not. "Already on
    // your board" is an answer for the user, not an error. When the crush
    // works, the board sends boardChanged, and every row's button updates.
    QString crushOutcomeText;
    jobPipelines.crushJobPosting(displayedJobPostings.at(rowIndex).jobPosting.jobPostingId,
                                 crushOutcomeText);
    return crushOutcomeText;
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

void DiscoveredJobListViewModel::addJobFromLink(const QString &pastedLink)
{
    discoveryJobScout.addJobFromLink(pastedLink);
}

void DiscoveredJobListViewModel::addJobFromCompanyAndTitle(const QString &companyName,
                                                           const QString &positionTitle)
{
    discoveryJobScout.addJobFromCompanyAndTitle(companyName, positionTitle);
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
    // Start from what Moonlight filled in, so the description it found is
    // kept, then apply the boxes as the user left them.
    JobPosting jobPostingFromForm = discoveryJobScout.filledFormFromPastedText();
    jobPostingFromForm.companyName = companyName;
    jobPostingFromForm.positionTitle = positionTitle;
    jobPostingFromForm.sourceUrl = sourceUrl;
    jobPostingFromForm.locationText = locationText;
    jobPostingFromForm.salaryText = salaryText;
    jobPostingFromForm.isRemoteRole = isRemoteRole;

    // A typed date is stored at noon, so a time zone change cannot move it
    // to the day before or after. A date that cannot be read is left empty.
    QDate postedDate;
    jobPostingFromForm.postedTimestamp =
        TypedDateReader::readTypedDate(postedText, QDate::currentDate(), QLocale(), postedDate)
            ? QDateTime(postedDate, timeOfDayForTypedDates)
            : QDateTime();
    discoveryJobScout.addJobFromFilledForm(jobPostingFromForm);
}
