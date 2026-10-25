#include "JobSearchProfileViewModel.h"

#include "../logic/jobscout/CareerPreset.h"

#include <QRegularExpression>

#include "../logic/jobscout/JobSearchProfile.h"
#include "../logic/jobscout/KnownWorkLocations.h"

namespace {

// One typed line becomes a list. The line is split at every comma,
// semicolon and newline, so a pasted column of skills works as well as a
// typed line. Each piece is trimmed, and empty pieces are dropped.
QStringList termListFromTypedLine(const QString &typedLine)
{
    static const QRegularExpression termSeparatorPattern(QStringLiteral("[,;\\n]"));
    QStringList typedTerms;
    for (const QString &rawTerm : typedLine.split(termSeparatorPattern, Qt::SkipEmptyParts)) {
        const QString tidiedTerm = rawTerm.trimmed();
        if (!tidiedTerm.isEmpty()) {
            typedTerms.append(tidiedTerm);
        }
    }
    return typedTerms;
}

QString typedLineFromTermList(const QStringList &terms)
{
    return terms.join(QStringLiteral(", "));
}

} // namespace

JobSearchProfileViewModel::JobSearchProfileViewModel(JobSearchProfile &searchProfile,
                                                     QObject *parent)
    : QObject(parent)
    , profile(searchProfile)
{
    connect(&profile, &JobSearchProfile::searchProfileChanged,
            this, &JobSearchProfileViewModel::searchProfileChanged);
}

QString JobSearchProfileViewModel::targetJobTitlesText() const
{
    return typedLineFromTermList(profile.targetJobTitles());
}

void JobSearchProfileViewModel::setTargetJobTitlesText(const QString &jobTitlesText)
{
    profile.setTargetJobTitles(termListFromTypedLine(jobTitlesText));
}

QString JobSearchProfileViewModel::skillKeywordsText() const
{
    return typedLineFromTermList(profile.skillKeywords());
}

void JobSearchProfileViewModel::setSkillKeywordsText(const QString &keywordsText)
{
    profile.setSkillKeywords(termListFromTypedLine(keywordsText));
}

QStringList JobSearchProfileViewModel::preferredWorkLocations() const
{
    return profile.preferredWorkLocations();
}

void JobSearchProfileViewModel::addWorkLocation(const QString &workLocation)
{
    profile.addPreferredWorkLocation(workLocation);
}

void JobSearchProfileViewModel::removeWorkLocationAt(int locationIndex)
{
    profile.removePreferredWorkLocationAt(locationIndex);
}

QStringList JobSearchProfileViewModel::workLocationSuggestions(
    const QString &typedPrefix) const
{
    return workLocationSuggestionsFor(typedPrefix, profile.preferredWorkLocations());
}

bool JobSearchProfileViewModel::remoteRolesOnly() const
{
    return profile.remoteRolesOnly();
}

void JobSearchProfileViewModel::setRemoteRolesOnly(bool remoteOnly)
{
    profile.setRemoteRolesOnly(remoteOnly);
}

QString JobSearchProfileViewModel::minimumSalaryText() const
{
    const int minimumSalary = profile.minimumAcceptableSalary();
    // Zero is "not a factor", and showing a literal 0 would read like a
    // number the user chose. An empty box tells the truth.
    return minimumSalary > 0 ? QString::number(minimumSalary) : QString();
}

void JobSearchProfileViewModel::setMinimumSalaryText(const QString &salaryText)
{
    // Keep whatever they typed usable: strip the punctuation people naturally
    // reach for ("$120,000") rather than rejecting the entry and making them
    // feel like they got it wrong.
    QString digitsOnlyText;
    for (const QChar &character : salaryText) {
        if (character.isDigit()) {
            digitsOnlyText.append(character);
        }
    }
    profile.setMinimumAcceptableSalary(digitsOnlyText.toInt());
}

bool JobSearchProfileViewModel::hasEnoughToRankBy() const
{
    return profile.hasEnoughToRankBy();
}

QString JobSearchProfileViewModel::addTheCppQtPreset()
{
    CareerPreset preset;
    QString problemText;
    if (!CareerPreset::readFromFile(shippedCppQtCareerPresetPath, preset, problemText)) {
        return problemText;
    }

    // Adds each preset term the list does not already have, compared without
    // case. The user's own entries stay first and in their order. Counts how
    // many were added, for the sentence at the end.
    auto listWithMissingTermsAdded = [](QStringList existingTerms,
                                        const QStringList &presetTerms, int &addedCount) {
        for (const QString &presetTerm : presetTerms) {
            if (!existingTerms.contains(presetTerm, Qt::CaseInsensitive)) {
                existingTerms.append(presetTerm);
                ++addedCount;
            }
        }
        return existingTerms;
    };

    int titlesAdded = 0;
    int skillsAdded = 0;
    profile.setTargetJobTitles(listWithMissingTermsAdded(profile.targetJobTitles(),
                                                         preset.jobTitles, titlesAdded));
    profile.setSkillKeywords(listWithMissingTermsAdded(profile.skillKeywords(),
                                                       preset.strongSkills + preset.goodSkills,
                                                       skillsAdded));
    emit searchProfileChanged();

    if (titlesAdded == 0 && skillsAdded == 0) {
        return QStringLiteral("Your profile already has everything in the C++ and Qt preset.");
    }
    return QStringLiteral("Added %1 job titles and %2 skills from the C++ and Qt preset. "
                          "Change or remove any of them above.")
        .arg(titlesAdded).arg(skillsAdded);
}
