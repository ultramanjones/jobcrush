#include "JobSearchProfileViewModel.h"

#include <QRegularExpression>

#include "../modelview/jobscout/JobSearchProfile.h"

namespace {

// One comma-separated line becomes a list. Splitting on commas AND newlines
// means a pasted column of skills works exactly as well as a typed line —
// people paste, and an app that punishes them for it is an app that adds
// friction for no reason.
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

QString JobSearchProfileViewModel::preferredLocationText() const
{
    return profile.preferredLocationText();
}

void JobSearchProfileViewModel::setPreferredLocationText(const QString &locationText)
{
    profile.setPreferredLocationText(locationText);
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
