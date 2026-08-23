#include "ProspectScorer.h"

#include <QDateTime>
#include <QRegularExpression>

#include "JobSearchProfile.h"

namespace {

// Words too common to mean anything when they appear in a job title. Matching
// "Senior" against "Senior Accountant" would be a false signal, and false
// signals in a ranking are worse than no ranking at all.
const QStringList meaninglessTitleWords = {
    QStringLiteral("senior"), QStringLiteral("junior"), QStringLiteral("lead"),
    QStringLiteral("staff"),  QStringLiteral("principal"), QStringLiteral("i"),
    QStringLiteral("ii"),     QStringLiteral("iii"),    QStringLiteral("the"),
    QStringLiteral("and"),    QStringLiteral("of"),     QStringLiteral("a"),
    QStringLiteral("mid"),    QStringLiteral("level"),  QStringLiteral("remote"),
};

// The role nouns that appear in half the job titles ever written. They are
// not meaningless — "Developer" genuinely separates a job from "Nurse" — but a
// target title that matched ONLY on one of these has told us nothing about
// whether the job fits. "JavaScript Developer" is not a hit for "Qt Developer".
const QStringList genericRoleWords = {
    QStringLiteral("developer"),   QStringLiteral("engineer"),
    QStringLiteral("programmer"),  QStringLiteral("manager"),
    QStringLiteral("analyst"),     QStringLiteral("designer"),
    QStringLiteral("architect"),   QStringLiteral("specialist"),
    QStringLiteral("consultant"),  QStringLiteral("administrator"),
    QStringLiteral("technician"),  QStringLiteral("scientist"),
    QStringLiteral("software"),
};

// Splits a phrase into the words worth matching on.
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

// True when the text contains the term as a WHOLE word. Substring matching
// would score "Java" against "JavaScript" — a mistake that would quietly
// poison every ranking it touched.
bool textContainsWholeTerm(const QString &loweredText, const QString &loweredTerm)
{
    const int termPosition = loweredText.indexOf(loweredTerm);
    if (termPosition < 0) {
        return false;
    }

    // A term made of symbols ("c++", "c#") has no word boundary to check on
    // its right-hand side, so finding it at all is the answer.
    const QChar lastTermCharacter = loweredTerm.at(loweredTerm.length() - 1);
    const bool termEndsInSymbol = !lastTermCharacter.isLetterOrNumber();

    const int characterBefore = termPosition - 1;
    const int characterAfter = termPosition + loweredTerm.length();

    const bool boundaryBefore = characterBefore < 0
        || !loweredText.at(characterBefore).isLetterOrNumber();
    const bool boundaryAfter = termEndsInSymbol
        || characterAfter >= loweredText.length()
        || !loweredText.at(characterAfter).isLetterOrNumber();

    return boundaryBefore && boundaryAfter;
}

} // namespace

ProspectScorer::ProspectScorer(const JobSearchProfile &searchProfile)
    : profile(searchProfile)
{
}

int ProspectScorer::scoreTitleMatch(const QString &loweredPositionTitle,
                                    QStringList &matchReasons) const
{
    const QStringList targetJobTitles = profile.targetJobTitles();
    if (targetJobTitles.isEmpty()) {
        return 0;
    }

    int bestTitleScore = 0;
    QString bestMatchingTitle;

    for (const QString &targetTitle : targetJobTitles) {
        const QString loweredTargetTitle = targetTitle.toLower().trimmed();
        if (loweredTargetTitle.isEmpty()) {
            continue;
        }

        int thisTitleScore = 0;

        if (loweredPositionTitle.contains(loweredTargetTitle)) {
            // The whole phrase, intact: the strongest thing a title can say.
            thisTitleScore = titleMatchMaximumPoints;
        } else {
            // Otherwise, how much of the target title survived? "C++ Engineer"
            // against "Software Engineer, C++" should still score well.
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

                // Proportional, and capped just under a full-phrase match so a
                // partial hit never ties an exact one.
                thisTitleScore = (titleMatchMaximumPoints - 5) * matchedWordCount
                                 / targetWords.count();

                // Matched on nothing but the generic role noun? That is the
                // difference between "Qt Developer" and "JavaScript
                // Developer", and it deserves a token score, not most of one.
                if (matchedDistinctiveWordCount == 0) {
                    thisTitleScore /= 4;
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
    } else if (bestTitleScore >= 8) {
        // Below this the overlap is too thin to be worth claiming — a reason
        // the user would disagree with costs more trust than it earns.
        matchReasons.append(QStringLiteral("Title overlaps %1").arg(bestMatchingTitle));
    }
    return bestTitleScore;
}

int ProspectScorer::scoreSkillMatch(const QString &loweredSearchableText,
                                    QStringList &matchReasons) const
{
    const QStringList skillKeywords = profile.skillKeywords();
    if (skillKeywords.isEmpty()) {
        return 0;
    }

    QStringList matchedSkills;
    for (const QString &skillKeyword : skillKeywords) {
        const QString loweredSkill = skillKeyword.toLower().trimmed();
        if (loweredSkill.isEmpty()) {
            continue;
        }
        // Each skill counts ONCE no matter how often the posting repeats it.
        // Otherwise a description that says "Qt" nine times would outrank a
        // job that genuinely wants everything the user can do.
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
    if (!profile.remoteRolesOnly()) {
        if (jobPosting.isRemoteRole) {
            matchReasons.append(QStringLiteral("Remote"));
            return remoteFitMaximumPoints;
        }
        // Remote is not required, so an on-site job is not penalized —
        // it simply earns nothing here.
        return 0;
    }

    if (jobPosting.isRemoteRole) {
        matchReasons.append(QStringLiteral("Remote, which is what you asked for"));
        return remoteFitMaximumPoints;
    }
    return 0;
}

int ProspectScorer::scoreLocationFit(const QString &loweredLocationText,
                                     QStringList &matchReasons) const
{
    const QString preferredLocation = profile.preferredLocationText().toLower().trimmed();
    if (preferredLocation.isEmpty() || loweredLocationText.isEmpty()) {
        return 0;
    }

    // Loose on purpose: boards write locations every possible way, and a
    // strict comparison would reject far more real matches than it caught.
    for (const QString &locationWord : significantWordsOf(preferredLocation)) {
        if (loweredLocationText.contains(locationWord)) {
            matchReasons.append(QStringLiteral("Location mentions %1")
                                    .arg(profile.preferredLocationText()));
            return locationFitMaximumPoints;
        }
    }
    return 0;
}

int ProspectScorer::scoreSalaryFit(const QString &salaryText,
                                   QStringList &matchReasons) const
{
    const int minimumSalary = profile.minimumAcceptableSalary();
    if (minimumSalary <= 0 || salaryText.isEmpty()) {
        return 0; // no floor set, or the posting is silent — no verdict either way
    }

    // Salary text is free-form ("$120,000 - $150,000 USD"). Pull out the
    // numbers and take the largest, which is the top of an advertised range.
    static const QRegularExpression salaryNumberPattern(QStringLiteral("[0-9][0-9,\\.]*"));
    QRegularExpressionMatchIterator numberIterator =
        salaryNumberPattern.globalMatch(salaryText);

    qint64 largestAdvertisedFigure = 0;
    while (numberIterator.hasNext()) {
        QString figureText = numberIterator.next().captured();
        figureText.remove(QLatin1Char(','));
        // Drop a decimal tail: "150.000" in some locales, "150.5k" in others —
        // neither is worth guessing at.
        figureText = figureText.section(QLatin1Char('.'), 0, 0);
        const qint64 figure = figureText.toLongLong();
        if (figure > largestAdvertisedFigure) {
            largestAdvertisedFigure = figure;
        }
    }

    // Some boards quote in thousands ("120k" arrives as 120). A figure that
    // small cannot be a yearly salary, so read it as thousands.
    if (largestAdvertisedFigure > 0 && largestAdvertisedFigure < 1000) {
        largestAdvertisedFigure *= 1000;
    }

    if (largestAdvertisedFigure >= minimumSalary) {
        matchReasons.append(QStringLiteral("Advertised pay clears your floor"));
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

    // A posting's odds fall off fast; this curve says so without pretending
    // to more precision than anyone has.
    if (daysSincePosted <= 1) {
        matchReasons.append(QStringLiteral("Posted today"));
        return freshnessMaximumPoints;
    }
    if (daysSincePosted <= 3) {
        matchReasons.append(QStringLiteral("Posted in the last few days"));
        return freshnessMaximumPoints - 1;
    }
    if (daysSincePosted <= 7) {
        return freshnessMaximumPoints - 2;
    }
    if (daysSincePosted <= 14) {
        return freshnessMaximumPoints - 3;
    }
    if (daysSincePosted <= 30) {
        return freshnessMaximumPoints - 4;
    }
    return 0;
}

ProspectMatchResult ProspectScorer::scoreJobPosting(const JobPosting &jobPosting) const
{
    ProspectMatchResult matchResult;

    const QString loweredPositionTitle = jobPosting.positionTitle.toLower();
    const QString loweredLocationText = jobPosting.locationText.toLower();

    // Skills are hunted in the title AND the description: a title says what
    // the job is called, the description says what it actually needs.
    const QString loweredSearchableText =
        loweredPositionTitle + QStringLiteral("\n") + jobPosting.fullDescriptionText.toLower();

    int totalScore = 0;
    totalScore += scoreTitleMatch(loweredPositionTitle, matchResult.matchReasons);
    totalScore += scoreSkillMatch(loweredSearchableText, matchResult.matchReasons);
    totalScore += scoreRemoteFit(jobPosting, matchResult.matchReasons);
    totalScore += scoreLocationFit(loweredLocationText, matchResult.matchReasons);
    totalScore += scoreSalaryFit(jobPosting.salaryText, matchResult.matchReasons);
    totalScore += scoreFreshness(jobPosting, matchResult.matchReasons);

    // "Remote only" is the user telling Job Crush something, not hinting at
    // it. A job that isn't remote sinks hard — but it is NOT hidden, because
    // boards mislabel this constantly and quietly burying a real match would
    // be the app overruling the person. It sinks; they still get to look.
    if (profile.remoteRolesOnly() && !jobPosting.isRemoteRole) {
        totalScore /= 2;
        matchResult.matchReasons.append(QStringLiteral("Not listed as remote"));
    }

    matchResult.matchScoreOutOfOneHundred = qBound(0, totalScore, 100);
    return matchResult;
}
