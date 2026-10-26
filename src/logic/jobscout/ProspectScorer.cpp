#include "ProspectScorer.h"

#include <QDateTime>
#include <QRegularExpression>

namespace {

// Words so common in job titles that a match on them tells nothing about the
// job. significantWordsOf drops these words. Without this list, the word
// "Senior" in a target title of "Senior Qt Developer" would match "Senior
// Accountant", and that job would get partial title points.
const QStringList meaninglessTitleWords = {
    QStringLiteral("senior"), QStringLiteral("junior"), QStringLiteral("lead"),
    QStringLiteral("staff"),  QStringLiteral("principal"), QStringLiteral("i"),
    QStringLiteral("ii"),     QStringLiteral("iii"),    QStringLiteral("the"),
    QStringLiteral("and"),    QStringLiteral("of"),     QStringLiteral("a"),
    QStringLiteral("mid"),    QStringLiteral("level"),  QStringLiteral("remote"),
};

// Common role words. Unlike the words in meaninglessTitleWords, these do tell
// something: a "Developer" job is different from a "Nurse" job. But if a
// posting's title matches a target title ONLY on these words, the match is
// weak, and scoreTitleMatch divides its points by 4. "JavaScript Developer"
// is not a good match for "Qt Developer".
const QStringList genericRoleWords = {
    QStringLiteral("developer"),   QStringLiteral("engineer"),
    QStringLiteral("programmer"),  QStringLiteral("manager"),
    QStringLiteral("analyst"),     QStringLiteral("designer"),
    QStringLiteral("architect"),   QStringLiteral("specialist"),
    QStringLiteral("consultant"),  QStringLiteral("administrator"),
    QStringLiteral("technician"),  QStringLiteral("scientist"),
    QStringLiteral("software"),
};

// Splits a phrase into lowercase words and keeps only the useful ones: words
// longer than one letter that are not in meaninglessTitleWords. Any character
// other than a-z, 0-9, "+" and "#" splits words. So "C++" and "C#" stay
// whole words.
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
//  2. Check the character right before it. It must be the start of the text,
//     or a character that is not a letter or a number.
//  3. Check the character right after it the same way. A term that ends in a
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

ProspectScorer::ProspectScorer(const ScoringProfile &scoringProfile)
    : searchProfile(scoringProfile)
{
}

ProspectScorer::ProspectScorer(const JobSearchProfile &jobSearchProfile)
    : searchProfile(jobSearchProfile.scoringProfile())
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
    const QStringList targetJobTitles = searchProfile.targetJobTitles;
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
            // If the posting's title contains the whole target title, it gets
            // full points.
            thisTitleScore = titleMatchMaximumPoints;
        } else {
            // If not, count how many of the target title's useful words
            // appear as whole words in the posting's title. For example, a
            // target of "C++ Engineer" and a posting of "Software Engineer,
            // C++" share both words, so the posting still scores well.
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

                // Give points in proportion to the share of words matched.
                // The most a partial match can get is titleMatchMaximumPoints
                // minus partialTitleMatchShortfall, so it always scores below
                // a full match.
                thisTitleScore = (titleMatchMaximumPoints - partialTitleMatchShortfall)
                                 * matchedWordCount / targetWords.count();

                // If every matched word is a common role word like
                // "Developer", divide the points by 4.
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

    // Add a title reason to the row. A full match says "direct match". A
    // partial match says "overlaps". If the title score is below
    // smallestTitleScoreWorthAReason (8), the points still count, but the row
    // gets no title reason, because the user would read it and disagree.
    if (bestTitleScore >= titleMatchMaximumPoints) {
        matchReasons.append(QStringLiteral("Title is a direct match for %1")
                                .arg(bestMatchingTitle));
    } else if (bestTitleScore >= smallestTitleScoreWorthAReason) {
        matchReasons.append(QStringLiteral("Title overlaps %1").arg(bestMatchingTitle));
    }
    return bestTitleScore;
}

int ProspectScorer::scoreSkillMatch(const QString &loweredSearchableText,
                                    QStringList &matchReasons) const
{
    const QStringList skillKeywords = searchProfile.skillKeywords;
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
    // A remote job gets remoteFitMaximumPoints. A job not listed as remote
    // gets 0 points here. If the user asked for remote jobs only, scoreJobPosting also cuts
    // the total score of a job that is not remote. That cut happens there,
    // once, not here.
    if (!jobPosting.isRemoteRole) {
        return 0;
    }
    matchReasons.append(searchProfile.remoteRolesOnly
                            ? QStringLiteral("Remote, which is what you asked for")
                            : QStringLiteral("Remote"));
    return remoteFitMaximumPoints;
}

QString ProspectScorer::matchedWorkLocationFor(const JobPosting &jobPosting) const
{
    const QStringList preferredWorkLocations = searchProfile.preferredWorkLocations;
    if (preferredWorkLocations.isEmpty() || jobPosting.locationText.isEmpty()) {
        return QString();
    }

    const QString loweredLocationText = jobPosting.locationText.toLower();

    // A posting matches one of the user's places if any one useful word of
    // that place appears in the posting's location text. So "Pittsburgh, PA"
    // also matches "Erie, PA", because the word "pa" matches. The match is
    // loose on purpose, because job sites write the same place many ways:
    // "Pittsburgh, PA", "Pittsburgh PA, USA", "Greater Pittsburgh Area". This
    // answer decides whether a job is hidden, so a loose match that shows a
    // few extra jobs is better than a strict one that hides real ones.
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
    if (searchProfile.preferredWorkLocations.isEmpty()) {
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
    const int minimumSalary = searchProfile.minimumAcceptableSalary;
    if (minimumSalary <= 0 || salaryText.isEmpty()) {
        return 0; // the user set no minimum salary, or the posting lists no pay
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

    // Some job sites write pay in thousands, such as "120k". The code above
    // reads that as 120, so multiply it by 1000.
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

    // If the user asked for remote jobs only, and this job is not listed as
    // remote, divide its total score by 2. This moves it far down the list,
    // but it is still shown, because job sites often label remote jobs wrong.
    // The user decides, not the app.
    if (searchProfile.remoteRolesOnly && !jobPosting.isRemoteRole) {
        totalScore /= notRemoteScoreDivisor;
        matchResult.matchReasons.append(QStringLiteral("Not listed as remote"));
    }

    matchResult.matchScoreOutOfOneHundred = qBound(0, totalScore, 100);
    return matchResult;
}
