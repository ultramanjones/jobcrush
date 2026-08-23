#include "JobSearchProfile.h"

#include <QSet>
#include <QSettings>

namespace {
const QString targetJobTitlesKey    = QStringLiteral("jobSearchProfile/targetJobTitles");
const QString skillKeywordsKey      = QStringLiteral("jobSearchProfile/skillKeywords");
const QString preferredWorkLocationsKey =
    QStringLiteral("jobSearchProfile/preferredWorkLocations");

// The single free-text location box this replaced. Read once, on the way to
// the new list, so nobody who already typed a location loses it.
const QString retiredSingleLocationKey =
    QStringLiteral("jobSearchProfile/preferredLocationText");
const QString remoteRolesOnlyKey    = QStringLiteral("jobSearchProfile/remoteRolesOnly");
const QString minimumSalaryKey      = QStringLiteral("jobSearchProfile/minimumAcceptableSalary");

// The user types "Qt Developer, C++ Engineer" in one box. Splitting here — in
// exactly one place — keeps the storage format and the typing format from
// drifting apart.
QStringList tidiedTermList(const QStringList &rawTerms)
{
    QStringList tidiedTerms;
    for (const QString &rawTerm : rawTerms) {
        const QString tidiedTerm = rawTerm.trimmed();
        if (!tidiedTerm.isEmpty() && !tidiedTerms.contains(tidiedTerm, Qt::CaseInsensitive)) {
            tidiedTerms.append(tidiedTerm);
        }
    }
    return tidiedTerms;
}
} // namespace

JobSearchProfile::JobSearchProfile(QObject *parent)
    : QObject(parent)
{
}

void JobSearchProfile::loadFromSettings()
{
    QSettings settings;
    storedTargetJobTitles = settings.value(targetJobTitlesKey).toStringList();
    storedSkillKeywords = settings.value(skillKeywordsKey).toStringList();
    storedPreferredWorkLocations = settings.value(preferredWorkLocationsKey).toStringList();

    // Carry over whatever was in the old single-line box, once. Splitting on
    // commas is exactly the thing that box got wrong, but it is still the
    // best reading of what is already stored there, and anything odd is one
    // click to remove now that entries are chips.
    if (storedPreferredWorkLocations.isEmpty()) {
        const QString retiredLocationText =
            settings.value(retiredSingleLocationKey).toString().trimmed();
        if (!retiredLocationText.isEmpty()) {
            storedPreferredWorkLocations =
                tidiedTermList(retiredLocationText.split(QLatin1Char(',')));
        }
    }
    storedRemoteRolesOnly = settings.value(remoteRolesOnlyKey, false).toBool();
    storedMinimumAcceptableSalary = settings.value(minimumSalaryKey, 0).toInt();

    emit searchProfileChanged();
}

void JobSearchProfile::persistToSettings() const
{
    QSettings settings;
    settings.setValue(targetJobTitlesKey, storedTargetJobTitles);
    settings.setValue(skillKeywordsKey, storedSkillKeywords);
    settings.setValue(preferredWorkLocationsKey, storedPreferredWorkLocations);
    settings.setValue(remoteRolesOnlyKey, storedRemoteRolesOnly);
    settings.setValue(minimumSalaryKey, storedMinimumAcceptableSalary);
}

QStringList JobSearchProfile::targetJobTitles() const
{
    return storedTargetJobTitles;
}

void JobSearchProfile::setTargetJobTitles(const QStringList &jobTitles)
{
    const QStringList tidiedTitles = tidiedTermList(jobTitles);
    if (tidiedTitles == storedTargetJobTitles) {
        return;
    }
    storedTargetJobTitles = tidiedTitles;
    persistToSettings();
    emit searchProfileChanged();
}

QStringList JobSearchProfile::skillKeywords() const
{
    return storedSkillKeywords;
}

void JobSearchProfile::setSkillKeywords(const QStringList &keywords)
{
    const QStringList tidiedKeywords = tidiedTermList(keywords);
    if (tidiedKeywords == storedSkillKeywords) {
        return;
    }
    storedSkillKeywords = tidiedKeywords;
    persistToSettings();
    emit searchProfileChanged();
}

QStringList JobSearchProfile::preferredWorkLocations() const
{
    return storedPreferredWorkLocations;
}

void JobSearchProfile::setPreferredWorkLocations(const QStringList &workLocations)
{
    const QStringList tidiedWorkLocations = tidiedTermList(workLocations);
    if (tidiedWorkLocations == storedPreferredWorkLocations) {
        return;
    }
    storedPreferredWorkLocations = tidiedWorkLocations;
    persistToSettings();
    emit searchProfileChanged();
}

void JobSearchProfile::addPreferredWorkLocation(const QString &workLocation)
{
    const QString tidiedWorkLocation = workLocation.trimmed();
    if (tidiedWorkLocation.isEmpty()
            || storedPreferredWorkLocations.contains(tidiedWorkLocation, Qt::CaseInsensitive)) {
        return; // nothing typed, or already on the list — quietly no-op
    }
    storedPreferredWorkLocations.append(tidiedWorkLocation);
    persistToSettings();
    emit searchProfileChanged();
}

void JobSearchProfile::removePreferredWorkLocationAt(int locationIndex)
{
    if (locationIndex < 0 || locationIndex >= storedPreferredWorkLocations.count()) {
        return;
    }
    storedPreferredWorkLocations.removeAt(locationIndex);
    persistToSettings();
    emit searchProfileChanged();
}

bool JobSearchProfile::remoteRolesOnly() const
{
    return storedRemoteRolesOnly;
}

void JobSearchProfile::setRemoteRolesOnly(bool remoteOnly)
{
    if (remoteOnly == storedRemoteRolesOnly) {
        return;
    }
    storedRemoteRolesOnly = remoteOnly;
    persistToSettings();
    emit searchProfileChanged();
}

int JobSearchProfile::minimumAcceptableSalary() const
{
    return storedMinimumAcceptableSalary;
}

void JobSearchProfile::setMinimumAcceptableSalary(int yearlySalary)
{
    const int sanitizedSalary = yearlySalary < 0 ? 0 : yearlySalary;
    if (sanitizedSalary == storedMinimumAcceptableSalary) {
        return;
    }
    storedMinimumAcceptableSalary = sanitizedSalary;
    persistToSettings();
    emit searchProfileChanged();
}

bool JobSearchProfile::hasEnoughToRankBy() const
{
    // One title or one skill is enough to say something meaningful. With
    // neither, any ranking would be theatre.
    return !storedTargetJobTitles.isEmpty() || !storedSkillKeywords.isEmpty();
}

QStringList JobSearchProfile::matchableTermsLowercased() const
{
    QStringList matchableTerms;
    for (const QString &term : storedTargetJobTitles + storedSkillKeywords) {
        const QString loweredTerm = term.trimmed().toLower();
        if (!loweredTerm.isEmpty() && !matchableTerms.contains(loweredTerm)) {
            matchableTerms.append(loweredTerm);
        }
    }
    return matchableTerms;
}
