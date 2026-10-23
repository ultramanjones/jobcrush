#pragma once

#include <QString>
#include <QStringList>

#include "../../model/JobPosting.h"

class JobSearchProfile;

// ProspectMatchResult
//
// How well one posting fits the user's search profile: a score out of 100,
// and the reasons for that score in plain English. The page shows the
// reasons on every row, so the user can see why a job ranked where it did
// and decide whether they agree.
struct ProspectMatchResult {
    int matchScoreOutOfOneHundred = 0;
    QStringList matchReasons;     // "Title matches Qt Developer", "4 of 6 skills"
};

// How strong a score is, for the page to color the score badge. The cutoffs
// live in ProspectScorer with the weights.
enum class ProspectMatchStrength {
    Weak,
    Fair,
    Strong
};

// ProspectScorer
//
// Ranks job postings for the Top Prospects tab. It compares a posting with
// the user's search profile using arithmetic and text matching only.
//
// It makes no network calls and uses no AI tokens, so it costs nothing to
// run. It works offline and with no AI brain connected. When the user edits
// their profile, every stored posting is scored again at once. The AI brain
// is never used to rank the list, because paying per token to sort a list of
// a thousand jobs would eat AI usage for no gain.
//
// It is a calculation with no state of its own, so it is a plain class in
// the Logic layer, not a QObject.
class ProspectScorer {
public:
    explicit ProspectScorer(const JobSearchProfile &searchProfile);

    ProspectMatchResult scoreJobPosting(const JobPosting &jobPosting) const;

    // Which of the user's chosen places this posting is in, or an empty
    // string when it is in none of them. This one answer decides two things:
    // how the posting scores on location, and whether it is shown at all.
    // Both use this function so they always agree.
    QString matchedWorkLocationFor(const JobPosting &jobPosting) const;

    // Whether this posting belongs in the user's search area. It does when:
    //  - the user named no places, so nothing is filtered,
    //  - the job is remote,
    //  - the posting names no location at all, or
    //  - its location matches one of the places the user named.
    bool jobPostingIsInsideSearchArea(const JobPosting &jobPosting) const;

    // Weak, fair or strong, by the cutoffs below.
    static ProspectMatchStrength matchStrengthFor(int matchScoreOutOfOneHundred);

private:
    // --- Every number the scoring uses, in one place ----------------------
    //
    // To tune the ranking, change these numbers. The arithmetic below uses
    // only these names.

    // The most points each part of the score can give. They add up to 100.
    static constexpr int titleMatchMaximumPoints  = 40; // the job title
    static constexpr int skillMatchMaximumPoints  = 35; // the user's skills
    static constexpr int remoteFitMaximumPoints   = 10;
    static constexpr int locationFitMaximumPoints =  5;
    static constexpr int salaryFitMaximumPoints   =  5;
    static constexpr int freshnessMaximumPoints   =  5; // how recently it was posted

    // A title that holds only some of the words of a target title scores at
    // most this many points less than a title that holds the whole phrase,
    // so a partial match never ties an exact one.
    static constexpr int partialTitleMatchShortfall = 5;

    // A title that matched only on a common word like "Developer" keeps this
    // fraction of its points. "JavaScript Developer" is not a good match for
    // "Qt Developer", so it gets a quarter.
    static constexpr int genericWordOnlyTitleDivisor = 4;

    // A title score below this is too weak to list as a reason on the row.
    static constexpr int smallestTitleScoreWorthAReason = 8;

    // A salary figure below this is read as thousands ("120k" arrives as
    // 120), because no yearly salary is that small.
    static constexpr int smallestFigureReadAsWholeDollars = 1000;

    // Freshness: a posting this many days old or newer gets the points beside
    // it. Older than the last row gets nothing.
    static constexpr int postedWithinOneDay    =  1; // all 5 points
    static constexpr int postedWithinThreeDays =  3; // 4 points
    static constexpr int postedWithinOneWeek   =  7; // 3 points
    static constexpr int postedWithinTwoWeeks  = 14; // 2 points
    static constexpr int postedWithinOneMonth  = 30; // 1 point

    // When the user asked for remote jobs only, a job not listed as remote
    // has its score divided by this. It is not hidden, because boards often
    // label remote jobs wrong.
    static constexpr int notRemoteScoreDivisor = 2;

    // Cutoffs for the score badge color on the page.
    static constexpr int strongMatchSmallestScore = 60;
    static constexpr int fairMatchSmallestScore   = 30;

    int scoreTitleMatch(const QString &loweredPositionTitle,
                        QStringList &matchReasons) const;
    int scoreSkillMatch(const QString &loweredSearchableText,
                        QStringList &matchReasons) const;
    int scoreRemoteFit(const JobPosting &jobPosting, QStringList &matchReasons) const;
    int scoreLocationFit(const JobPosting &jobPosting, QStringList &matchReasons) const;
    int scoreSalaryFit(const QString &salaryText, QStringList &matchReasons) const;
    int scoreFreshness(const JobPosting &jobPosting, QStringList &matchReasons) const;

    const JobSearchProfile &searchProfile;
};
