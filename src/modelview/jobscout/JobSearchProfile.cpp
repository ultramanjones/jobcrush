#include "JobSearchProfile.h"

#include <QSet>
#include <QSettings>

namespace {
const QString targetJobTitlesKey    = QStringLiteral("jobSearchProfile/targetJobTitles");
const QString skillKeywordsKey      = QStringLiteral("jobSearchProfile/skillKeywords");
const QString preferredLocationKey  = QStringLiteral("jobSearchProfile/preferredLocationText");
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
    storedPreferredLocationText = settings.value(preferredLocationKey).toString();
    storedRemoteRolesOnly = settings.value(remoteRolesOnlyKey, false).toBool();
    storedMinimumAcceptableSalary = settings.value(minimumSalaryKey, 0).toInt();

    emit searchProfileChanged();
}

void JobSearchProfile::persistToSettings() const
{
    QSettings settings;
    settings.setValue(targetJobTitlesKey, storedTargetJobTitles);
    settings.setValue(skillKeywordsKey, storedSkillKeywords);
    settings.setValue(preferredLocationKey, storedPreferredLocationText);
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

QString JobSearchProfile::preferredLocationText() const
{
    return storedPreferredLocationText;
}

void JobSearchProfile::setPreferredLocationText(const QString &locationText)
{
    const QString tidiedLocationText = locationText.trimmed();
    if (tidiedLocationText == storedPreferredLocationText) {
        return;
    }
    storedPreferredLocationText = tidiedLocationText;
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
