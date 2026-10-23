#include "ProspectScorer.h"

#include <QDateTime>
#include <QRegularExpression>

#include "JobSearchProfile.h"

namespace {

// Words that appear in so many job titles that matching on them says nothing.
// Without this list, "Senior" in the user's target title would match "Senior
// Accountant", and that job would rank as a partial title match.
const QStringList meaninglessTitleWords = {
    QStringLiteral("senior"), QStringLiteral("junior"), QStringLiteral("lead"),
    QStringLiteral("staff"),  QStringLiteral("principal"), QStringLiteral("i"),
    QStringLiteral("ii"),     QStringLiteral("iii"),    QStringLiteral("the"),
    QStringLiteral("and"),    QStringLiteral("of"),     QStringLiteral("a"),
    QStringLiteral("mid"),    QStringLiteral("level"),  QStringLiteral("remote"),
};

// Common role words. Unlike the words above, these do mean something:
// "Developer" is a different job from "Nurse". But a title that matches a
// target title ONLY on one of these words is a weak match. "JavaScript
// Developer" is not a good match for "Qt Developer".
const QStringList genericRoleWords = {
    QStringLiteral("developer"),   QStringLiteral("engineer"),
    QStringLiteral("programmer"),  QStringLiteral("manager"),
    QStringLiteral("analyst"),     QStringLiteral("designer"),
    QStringLiteral("architect"),   QStringLiteral("specialist"),
    QStringLiteral("consultant"),  QStringLiteral("administrator"),
    QStringLiteral("technician"),  QStringLiteral("scientist"),
    QStringLiteral("software"),
};

// Splits a phrase into the words worth matching on: lowercase, longer than
// one letter, and not in meaninglessTitleWords. Plus and hash signs count as
// part of a word, so "C++" and "C#" stay whole.
QStringList significantWordsOf(const QString &phrase)
{
    static const QRegularExpression nonWordCharacterPattern(QStringLiteral("[^a-z0-9+#]+"));
    QStringList significantWords;
    for (const QString &word : phrase.toLower().split(nonWordCharacterPattern,
                                                      Qt::SkipEmptyParts)) {
        if (word.length() > 1 && !meaninglessTitleWords.contains(word)) {
            significantWords.append(word);
        }
    }
    return significantWords;
}

// True when the text contains the term as a WHOLE word, anywhere in the text.
// Both strings must already be lowercase.
//
// How it works:
//  1. Find the next place the term appears, starting from the left.
//  2. Check the character just before it. It must be the start of the text,
//     or a character that is not a letter or a number.
//  3. Check the character just after it the same way. A term that ends in a
//     symbol, like "c++" or "c#", skips this check, because the symbol is
//     already the end of the word.
//  4. If both checks pass, the term is there as a whole word. If not, search
//     again from one character further on.
//
// Checking every place the term appears matters. In "javascript and java",
// the first "java" is part of "javascript" and fails, but the second one is
// a whole word and must count.
bool textContainsWholeTerm(const QString &loweredText, const QString &loweredTerm)
{
    if (loweredTerm.isEmpty()) {
        return false;
    }

    const QChar lastTermCharacter = loweredTerm.at(loweredTerm.length() - 1);
    const bool termEndsInSymbol = !lastTermCharacter.isLetterOrNumber();

    int searchFromPosition = 0;
    while (true) {
        const int termPosition = loweredText.indexOf(loweredTerm, searchFromPosition);
        if (termPosition < 0) {
            return false;
        }

        const int positionBefore = termPosition - 1;
        const int positionAfter = termPosition + loweredTerm.length();

        const bool wordStartsHere = positionBefore < 0
            || !loweredText.at(positionBefore).isLetterOrNumber();
        const bool wordEndsHere = termEndsInSymbol
            || positionAfter >= loweredText.length()
            || !loweredText.at(positionAfter).isLetterOrNumber();

        if (wordStartsHere && wordEndsHere) {
            return true;
        }
        searchFromPosition = termPosition + 1;
    }
}

} // namespace

ProspectScorer::ProspectScorer(const JobSearchProfile &searchProfile)
    : searchProfile(searchProfile)
{
}

ProspectMatchStrength ProspectScorer::matchStrengthFor(int matchScoreOutOfOneHundred)
{
    if (matchScoreOutOfOneHundred >= strongMatchSmallestScore) {
        return ProspectMatchStrength::Strong;
    }
    if (matchScoreOutOfOneHundred >= fairMatchSmallestScore) {
        return ProspectMatchStrength::Fair;
    }
    return ProspectMatchStrength::Weak;
}

int ProspectScorer::scoreTitleMatch(const QString &loweredPositionTitle,
                                    QStringList &matchReasons) const
{
    const QStringList targetJobTitles = searchProfile.targetJobTitles();
    if (targetJobTitles.isEmpty()) {
        return 0;
    }

    // Each target title is scored on its own, and the best one counts.
    int bestTitleScore = 0;
    QString bestMatchingTitle;

    for (const QString &targetTitle : targetJobTitles) {
        const QString loweredTargetTitle = targetTitle.toLower().trimmed();
        if (loweredTargetTitle.isEmpty()) {
            continue;
        }

        int thisTitleScore = 0;

        if (loweredPositionTitle.contains(loweredTargetTitle)) {
            // The posting's title holds the whole target title: full points.
            thisTitleScore = titleMatchMaximumPoints;
        } else {
            // Otherwise, count how many of the target title's words are in
            // the posting's title. "C++ Engineer" against "Software Engineer,
            // C++" should still score well.
            const QStringList targetWords = significantWordsOf(loweredTargetTitle);
            if (!targetWords.isEmpty()) {
                int matchedWordCount = 0;
                int matchedDistinctiveWordCount = 0;
                for (const QString &targetWord : targetWords) {
                    if (!textContainsWholeTerm(loweredPositionTitle, targetWord)) {
                        continue;
                    }
                    ++matchedWordCount;
                    if (!genericRoleWords.contains(targetWord)) {
                        ++matchedDistinctiveWordCount;
                    }
                }

                // Points in proportion to the words matched, out of a maximum
                // a little below a full-phrase match.
                thisTitleScore = (titleMatchMaximumPoints - partialTitleMatchShortfall)
                                 * matchedWordCount / targetWords.count();

                // Matched only on a common role word like "Developer".
                if (matchedDistinctiveWordCount == 0) {
                    thisTitleScore /= genericWordOnlyTitleDivisor;
                }
            }
        }

        if (thisTitleScore > bestTitleScore) {
            bestTitleScore = thisTitleScore;
            bestMatchingTitle = targetTitle;
        }
    }

    if (bestTitleScore >= titleMatchMaximumPoints) {
        matchReasons.append(QStringLiteral("Title is a direct match for %1")
                                .arg(bestMatchingTitle));
    } else if (bestTitleScore >= smallestTitleScoreWorthAReason) {
        // A weaker match gets no reason on the row, because the user would
        // read it and disagree.
        matchReasons.append(QStringLiteral("Title overlaps %1").arg(bestMatchingTitle));
    }
    return bestTitleScore;
}

int ProspectScorer::scoreSkillMatch(const QString &loweredSearchableText,
                                    QStringList &matchReasons) const
{
    const QStringList skillKeywords = searchProfile.skillKeywords();
    if (skillKeywords.isEmpty()) {
        return 0;
    }

    QStringList matchedSkills;
    for (const QString &skillKeyword : skillKeywords) {
        const QString loweredSkill = skillKeyword.toLower().trimmed();
        if (loweredSkill.isEmpty()) {
            continue;
        }
        // Each skill counts once, however often the posting repeats it.
        // Otherwise a posting that says "Qt" nine times would outrank one
        // that asks for every skill the user has.
        if (textContainsWholeTerm(loweredSearchableText, loweredSkill)) {
            matchedSkills.append(skillKeyword);
        }
    }

    if (matchedSkills.isEmpty()) {
        return 0;
    }

    matchReasons.append(QStringLiteral("Mentions %1 of your %2 skills: %3")
                            .arg(matchedSkills.count())
                            .arg(skillKeywords.count())
                            .arg(matchedSkills.join(QStringLiteral(", "))));

    return skillMatchMaximumPoints * matchedSkills.count() / skillKeywords.count();
}

int ProspectScorer::scoreRemoteFit(const JobPosting &jobPosting,
                                   QStringList &matchReasons) const
{
    // An on-site job earns nothing here, but loses nothing either. The
    // penalty for "remote only" is applied once, in scoreJobPosting.
    if (!jobPosting.isRemoteRole) {
        return 0;
    }
    matchReasons.append(searchProfile.remoteRolesOnly()
                            ? QStringLiteral("Remote, which is what you asked for")
                            : QStringLiteral("Remote"));
    return remoteFitMaximumPoints;
}

QString ProspectScorer::matchedWorkLocationFor(const JobPosting &jobPosting) const
{
    const QStringList preferredWorkLocations = searchProfile.preferredWorkLocations();
    if (preferredWorkLocations.isEmpty() || jobPosting.locationText.isEmpty()) {
        return QString();
    }

    const QString loweredLocationText = jobPosting.locationText.toLower();

    // Matching any one of the user's places is enough. The match is loose on
    // purpose, because boards write the same place many ways: "Pittsburgh,
    // PA", "Pittsburgh PA, USA", "Greater Pittsburgh Area". This answer
    // decides whether a job is hidden, so a loose match that shows a few
    // extra jobs is better than a strict one that hides real ones.
    for (const QString &preferredLocation : preferredWorkLocations) {
        for (const QString &locationWord : significantWordsOf(preferredLocation)) {
            // Words of three letters or fewer are usually state codes ("PA",
            // "TX"). They must match as whole words, or "PA" would match
            // "Palo Alto" and "Panama".
            const bool wordMatched = locationWord.length() <= 3
                ? textContainsWholeTerm(loweredLocationText, locationWord)
                : loweredLocationText.contains(locationWord);
            if (wordMatched) {
                return preferredLocation;
            }
        }
    }
    return QString();
}

bool ProspectScorer::jobPostingIsInsideSearchArea(const JobPosting &jobPosting) const
{
    // The user named no places, so nothing is filtered.
    if (searchProfile.preferredWorkLocations().isEmpty()) {
        return true;
    }

    // A remote job can be done from anywhere, so it is inside every search
    // area. This holds even when the posting also names a head office far
    // away. People who can only work in one place want these jobs most.
    if (jobPosting.isRemoteRole) {
        return true;
    }

    // A posting that names no location is shown. The filter is for jobs in
    // the wrong city, and this posting has not said it is in one.
    //
    // This matters most for a job the user typed in by hand with a company
    // and a title but no location. Hiding it would look like the app lost it.
    if (jobPosting.locationText.trimmed().isEmpty()) {
        return true;
    }

    return !matchedWorkLocationFor(jobPosting).isEmpty();
}

int ProspectScorer::scoreLocationFit(const JobPosting &jobPosting,
                                     QStringList &matchReasons) const
{
    const QString matchedLocation = matchedWorkLocationFor(jobPosting);
    if (matchedLocation.isEmpty()) {
        return 0;
    }
    matchReasons.append(QStringLiteral("Location matches %1").arg(matchedLocation));
    return locationFitMaximumPoints;
}

int ProspectScorer::scoreSalaryFit(const QString &salaryText,
                                   QStringList &matchReasons) const
{
    const int minimumSalary = searchProfile.minimumAcceptableSalary();
    if (minimumSalary <= 0 || salaryText.isEmpty()) {
        return 0; // no floor set, or the posting gives no pay
    }

    // Salary text is free-form, like "$120,000 - $150,000 USD". Pull out
    // every number and keep the largest, which is the top of the range.
    static const QRegularExpression salaryNumberPattern(QStringLiteral("[0-9][0-9,\\.]*"));
    QRegularExpressionMatchIterator numberIterator =
        salaryNumberPattern.globalMatch(salaryText);

    qint64 largestAdvertisedFigure = 0;
    while (numberIterator.hasNext()) {
        QString figureText = numberIterator.next().captured();
        figureText.remove(QLatin1Char(','));
        // Drop anything after a period. "150.000" is 150 thousand in some
        // countries, and "150.5k" is 150.5 thousand. Neither can be read
        // safely, and the whole-number part is close enough.
        figureText = figureText.section(QLatin1Char('.'), 0, 0);
        const qint64 figure = figureText.toLongLong();
        if (figure > largestAdvertisedFigure) {
            largestAdvertisedFigure = figure;
        }
    }

    // Some boards give pay in thousands, so "120k" arrives here as 120.
    if (largestAdvertisedFigure > 0
            && largestAdvertisedFigure < smallestFigureReadAsWholeDollars) {
        largestAdvertisedFigure *= 1000;
    }

    if (largestAdvertisedFigure >= minimumSalary) {
        matchReasons.append(QStringLiteral("Advertised pay meets your minimum"));
        return salaryFitMaximumPoints;
    }
    return 0;
}

int ProspectScorer::scoreFreshness(const JobPosting &jobPosting,
                                   QStringList &matchReasons) const
{
    if (!jobPosting.postedTimestamp.isValid()) {
        return 0;
    }

    const qint64 daysSincePosted =
        jobPosting.postedTimestamp.daysTo(QDateTime::currentDateTime());

    // Older postings are more likely to be filled already, so newer ones
    // score higher.
    if (daysSincePosted <= postedWithinOneDay) {
        matchReasons.append(QStringLiteral("Posted in the last day"));
        return freshnessMaximumPoints;
    }
    if (daysSincePosted <= postedWithinThreeDays) {
        matchReasons.append(QStringLiteral("Posted in the last few days"));
        return freshnessMaximumPoints - 1;
    }
    if (daysSincePosted <= postedWithinOneWeek) {
        return freshnessMaximumPoints - 2;
    }
    if (daysSincePosted <= postedWithinTwoWeeks) {
        return freshnessMaximumPoints - 3;
    }
    if (daysSincePosted <= postedWithinOneMonth) {
        return freshnessMaximumPoints - 4;
    }
    return 0;
}

ProspectMatchResult ProspectScorer::scoreJobPosting(const JobPosting &jobPosting) const
{
    ProspectMatchResult matchResult;

    const QString loweredPositionTitle = jobPosting.positionTitle.toLower();

    // Skills are looked for in the title AND the description. The title says
    // what the job is called. The description says what it needs.
    const QString loweredSearchableText =
        loweredPositionTitle + QStringLiteral("\n") + jobPosting.fullDescriptionText.toLower();

    int totalScore = 0;
    totalScore += scoreTitleMatch(loweredPositionTitle, matchResult.matchReasons);
    totalScore += scoreSkillMatch(loweredSearchableText, matchResult.matchReasons);
    totalScore += scoreRemoteFit(jobPosting, matchResult.matchReasons);
    totalScore += scoreLocationFit(jobPosting, matchResult.matchReasons);
    totalScore += scoreSalaryFit(jobPosting.salaryText, matchResult.matchReasons);
    totalScore += scoreFreshness(jobPosting, matchResult.matchReasons);

    // The user asked for remote jobs only, and this one is not listed as
    // remote. It drops far down the list but is still shown, because boards
    // often label remote jobs wrong. The user decides, not the app.
    if (searchProfile.remoteRolesOnly() && !jobPosting.isRemoteRole) {
        totalScore /= notRemoteScoreDivisor;
        matchResult.matchReasons.append(QStringLiteral("Not listed as remote"));
    }

    matchResult.matchScoreOutOfOneHundred = qBound(0, totalScore, 100);
    return matchResult;
}
