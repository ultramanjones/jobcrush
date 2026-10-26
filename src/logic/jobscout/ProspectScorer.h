#pragma once

#include <QString>
#include <QStringList>

#include "../../model/JobPosting.h"
#include "JobSearchProfile.h"

// ProspectMatchResult
//
// How well one job posting fits the user's search profile.
// matchScoreOutOfOneHundred is a score from 0 to 100. The screen shows it to
// the user as a percent, for example "46%". matchReasons lists the reasons
// for the score in plain English. The page shows the reasons on every row,
// so the user can see why a job ranked where it did and decide whether they
// agree.
struct ProspectMatchResult {
    int matchScoreOutOfOneHundred = 0;
    // For example: "Title is a direct match for Qt Developer" and
    // "Mentions 4 of your 6 skills: C++, Qt, QML, Linux".
    QStringList matchReasons;
};

// A posting and its score, kept together. One list of these is simpler than
// two lists the caller must keep in the same order.
struct ScoredJobPosting {
    JobPosting jobPosting;
    ProspectMatchResult matchResult;
};

// How strong a match score is: weak, fair or strong. The page uses this to
// pick the color of the score badge on each row.
// ProspectScorer::matchStrengthFor picks the strength. The cutoff scores are
// strongMatchSmallestScore and fairMatchSmallestScore, in ProspectScorer.
enum class ProspectMatchStrength {
    Weak,
    Fair,
    Strong
};

// ProspectScorer
//
// ProspectScorer gives each job posting a match score from 0 to 100. The
// screen shows the score to the user as a percent. The score compares the
// posting with the user's search profile: target job titles, skills, remote
// only, places, and lowest salary. The Top Prospects and Outside your area
// tabs sort jobs by this score. ProspectScorer also runs the location
// filter, in jobPostingIsInsideSearchArea. The location filter hides a job
// from the normal tabs when its location does not match a place the user
// named.
//
// It uses only arithmetic and text matching. It makes no network calls and
// uses no AI tokens, so it costs nothing to run. It works offline and with
// no AI brain connected. When the user edits their profile, the page asks
// for its rows again, and every job on the tab is scored again right away.
// The AI brain is never used to rank the list, because paying per token to
// sort a list of a thousand jobs would eat AI usage for no gain.
//
// Nothing in it changes after it is built, so it is a plain class in the
// Logic layer, not a QObject. It keeps its own copy of the profile, so a
// background thread can use it safely.
class ProspectScorer {
public:
    explicit ProspectScorer(const ScoringProfile &scoringProfile);
    explicit ProspectScorer(const JobSearchProfile &jobSearchProfile);

    ProspectMatchResult scoreJobPosting(const JobPosting &jobPosting) const;

    // Returns the first of the user's places that matches the posting's
    // location text. Returns an empty string if none match, if the user
    // named no places, or if the posting has no location text. Two things use
    // this answer: the location points in the score, and the location
    // filter. Both call this one function so they always agree.
    QString matchedWorkLocationFor(const JobPosting &jobPosting) const;

    // This is the location filter. It returns true if the posting should
    // show on the normal tabs. It returns false if the posting should show
    // only on the Outside your area tab. It returns true when any of these
    // is true:
    //  - the user named no places, so nothing is filtered,
    //  - the job is remote,
    //  - the posting names no location at all, or
    //  - its location matches one of the places the user named.
    bool jobPostingIsInsideSearchArea(const JobPosting &jobPosting) const;

    // Returns Strong for a score of strongMatchSmallestScore or more, Fair
    // for a score of fairMatchSmallestScore or more, and Weak for anything
    // lower.
    static ProspectMatchStrength matchStrengthFor(int matchScoreOutOfOneHundred);

private:
    // --- Every number the scoring uses, in one place ----------------------
    //
    // To change how jobs rank, change these numbers. The scoring code in
    // ProspectScorer.cpp reads them by name.

    // The most points each part of the score can give. They add up to 100.
    static constexpr int titleMatchMaximumPoints  = 40; // the job title
    static constexpr int skillMatchMaximumPoints  = 35; // the user's skills
    static constexpr int remoteFitMaximumPoints   = 10;
    static constexpr int locationFitMaximumPoints =  5;
    static constexpr int salaryFitMaximumPoints   =  5;
    static constexpr int freshnessMaximumPoints   =  5; // how recently it was posted

    // If the posting's title does not hold a whole target title, it can
    // still get points for each target word it holds. That partial match
    // can score at most titleMatchMaximumPoints minus this number. So a
    // partial match always scores below a full match.
    static constexpr int partialTitleMatchShortfall = 5;

    // If a partial title match found only common role words like
    // "Developer", its title points are divided by this number. For example,
    // "JavaScript Developer" shares only "Developer" with "Qt Developer", so
    // it keeps a quarter of its points.
    static constexpr int genericWordOnlyTitleDivisor = 4;

    // If the title score is below this, the row does not list the title as a
    // reason. The points still count in the score.
    static constexpr int smallestTitleScoreWorthAReason = 8;

    // If the largest number in the salary text is below this, the scorer
    // reads it as thousands of dollars and multiplies it by 1000. For
    // example, "120k" is read as 120, which becomes 120,000. No yearly salary
    // is less than 1000 dollars.
    static constexpr int smallestFigureReadAsWholeDollars = 1000;

    // Freshness points by age. A posting this many days old or newer gets
    // the points shown beside it. A posting older than postedWithinOneMonth
    // days gets 0 points.
    static constexpr int postedWithinOneDay    =  1; // all 5 points
    static constexpr int postedWithinThreeDays =  3; // 4 points
    static constexpr int postedWithinOneWeek   =  7; // 3 points
    static constexpr int postedWithinTwoWeeks  = 14; // 2 points
    static constexpr int postedWithinOneMonth  = 30; // 1 point

    // If the user asked for remote jobs only, and a job is not listed as
    // remote, its total score is divided by this number. The job is not
    // hidden, because job sites often label remote jobs wrong.
    static constexpr int notRemoteScoreDivisor = 2;

    // The lowest score that counts as a strong match, and the lowest score
    // that counts as a fair match. The page colors the score badge by these.
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

    const ScoringProfile searchProfile;
};
