#pragma once

#include <QString>
#include <QStringList>

#include "../../model/JobPosting.h"

class JobSearchProfile;

// ProspectMatchResult
//
// One posting's standing against the user's profile: a number out of 100 and,
// just as importantly, the plain-English reasons behind it.
//
// The reasons are not decoration. A ranking nobody can question is a ranking
// nobody can trust, and a job search is too consequential to hand someone an
// unexplained order and expect them to act on it.
struct ProspectMatchResult {
    int matchScoreOutOfOneHundred = 0;
    QStringList matchReasons;     // "Title matches Qt Developer", "4 of 6 skills"
};

// ProspectScorer
//
// The algorithm behind Top Prospects. Ranks a posting against the search
// profile using arithmetic and string matching — nothing else.
//
// It costs NOTHING to run: no API calls, no AI tokens, no network. It works
// with no brain configured at all, it works offline, and re-ranking every
// posting when the profile changes is instant. AIBrain stays available for
// deep analysis of a SINGLE posting the user has chosen to look at; it is
// deliberately never the ranking engine, because paying per token to sort a
// list is the wrong shape for this problem.
//
// A calculation with no state of its own, so it lives in ModelView as the
// architecture requires, and stays a plain class rather than a QObject.
class ProspectScorer {
public:
    explicit ProspectScorer(const JobSearchProfile &searchProfile);

    ProspectMatchResult scoreJobPosting(const JobPosting &jobPosting) const;

    // Which of the user's chosen places this posting sits in, or empty when
    // none of them. Public because the SAME answer decides two things — how a
    // posting scores and whether it is shown at all — and those two must
    // never be able to disagree about it.
    QString matchedWorkLocationFor(const JobPosting &jobPosting) const;

    // Whether this posting belongs in the user's search area at all.
    //
    // Three ways to be inside, and the first two matter as much as the third:
    //  - the user named no places, so nothing is being filtered
    //  - the job is remote, which is nowhere and everywhere
    //  - its location matches somewhere they named
    bool jobPostingIsInsideSearchArea(const JobPosting &jobPosting) const;

private:
    // --- The weights, all in one readable place ---------------------------
    //
    // They add to 100. Tuning the algorithm means editing these numbers and
    // nothing else, which is the entire reason they are named constants
    // sitting together instead of magic numbers buried in the arithmetic.
    static constexpr int titleMatchMaximumPoints    = 40; // the strongest signal by far
    static constexpr int skillMatchMaximumPoints    = 35; // what the user actually brings
    static constexpr int remoteFitMaximumPoints     = 10;
    static constexpr int locationFitMaximumPoints   =  5;
    static constexpr int salaryFitMaximumPoints     =  5;
    static constexpr int freshnessMaximumPoints     =  5; // a stale posting is often filled

    int scoreTitleMatch(const QString &loweredPositionTitle,
                        QStringList &matchReasons) const;
    int scoreSkillMatch(const QString &loweredSearchableText,
                        QStringList &matchReasons) const;
    int scoreRemoteFit(const JobPosting &jobPosting, QStringList &matchReasons) const;
    int scoreLocationFit(const JobPosting &jobPosting, QStringList &matchReasons) const;
    int scoreSalaryFit(const QString &salaryText, QStringList &matchReasons) const;
    int scoreFreshness(const JobPosting &jobPosting, QStringList &matchReasons) const;

    const JobSearchProfile &profile;
};
