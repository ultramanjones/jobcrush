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

// A date the user types, such as the date a job was posted, is stored at
// noon on that day. If it were stored at midnight, a time zone change could
// move it to the day before. Noon is twelve hours from either end of the
// day, so the date stays the same.
const QTime timeOfDayForTypedDates(12, 0);

// The names of the two special tabs, Manual Add and Outside your area.
// Every other tab is named by a job site's storage name. Each storage name
// is a plain word with no hyphen. These two names have hyphens, so they can
// never match a job site's storage name.
const QString manualAddTabName = QStringLiteral("jobcrush-manual-add");
const QString outsideSearchAreaTabName = QStringLiteral("jobcrush-outside-area");

// Returns the heading the page shows above a day's group of rows. It uses
// the date the job was posted. If the posting gave no date, it uses the date
// Job Crush found the job. Today and yesterday are shown as words, because
// "Today" is easier to read than a date. Older days are shown as a date,
// such as "August 21". If there is no date at all, it returns "No date
// given".
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

// Returns the name to show for the place a posting came from. That place is
// either a job SITE that Job Crush sweeps, such as Remotive, or an employer's
// job BOARD that it reads, such as Ashby. Sites and boards are listed in two
// different places. This function checks the site list first, then the
// board list. An earlier version checked only the site list. Then "ashby"
// showed on screen in lowercase, because Ashby is a board, not a site.
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

    // Neither list knows this name. Show the stored name as it is. A blank
    // space would look like a bug to the user.
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

    // The user can put a job on the board, or take it off, from this page or
    // from the Job Pipelines page. Either way, JobPipelines sends
    // boardChanged. Then every row has to update its CRUSH button.
    connect(&jobPipelines, &JobPipelines::boardChanged,
            this, &DiscoveredJobListViewModel::refreshIsOnTheBoardForEveryRow);

    requestRowsForTheActiveTab();
}

void DiscoveredJobListViewModel::requestRowsForTheActiveTab()
{
    // The "Outside your area" tab exists only while the user has named at
    // least one place to search in. If the user removes the last place in
    // Settings while that tab is showing, the tab goes away. So this code
    // switches back to Top Prospects. Otherwise the page would show an empty
    // list under a tab that no longer exists.
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

    // This replaces the whole list at once. That is simple, and it is fast
    // enough, because the background thread already did the slow scoring.
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

    // This clears the old tab's rows right away. If it waited, the old rows
    // would show under the new tab's name until the new rows were ready.
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

    // This opens only web links, which start with http or https. Most links
    // come from job boards, and Job Crush does not control what they send. A
    // link that starts with file:// or with another program's name could make
    // the computer open a file or start a program instead of a web page.
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

    // JobPipelines fills in crushOutcomeText whether the crush worked or not.
    // If the job is already on the board, the text says so. That is an answer
    // for the user, not an error. If the crush works, JobPipelines sends
    // boardChanged, and every row's CRUSH button updates.
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
    // Start from the posting Moonlight filled in. That keeps the job
    // description Moonlight found, which this function does not receive.
    // Then copy in the value of each box as the user left it.
    JobPosting jobPostingFromForm = discoveryJobScout.filledFormFromPastedText();
    jobPostingFromForm.companyName = companyName;
    jobPostingFromForm.positionTitle = positionTitle;
    jobPostingFromForm.sourceUrl = sourceUrl;
    jobPostingFromForm.locationText = locationText;
    jobPostingFromForm.salaryText = salaryText;
    jobPostingFromForm.isRemoteRole = isRemoteRole;

    // The typed date is stored at noon. If it were stored at midnight, a
    // time zone change could move it to the day before. Noon is twelve hours
    // from either end of the day, so the date stays the same. If
    // TypedDateReader cannot read the date, the posted date is left empty.
    QDate postedDate;
    jobPostingFromForm.postedTimestamp =
        TypedDateReader::readTypedDate(postedText, QDate::currentDate(), QLocale(), postedDate)
            ? QDateTime(postedDate, timeOfDayForTypedDates)
            : QDateTime();
    discoveryJobScout.addJobFromFilledForm(jobPostingFromForm);
}
