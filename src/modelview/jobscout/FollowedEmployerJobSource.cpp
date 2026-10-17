#include "FollowedEmployerJobSource.h"

#include <memory>

#include <QMetaObject>
#include <QSet>
#include <QStringList>

#include "EmployerBoardHttp.h"
#include "FollowedEmployerRoster.h"
#include "JobScoutReply.h"
#include "JobSearchProfile.h"

namespace {

const QString followedEmployersStorageName = QStringLiteral("employerboards");

// One sweep across every watched company.
//
// Held by shared_ptr and captured by the board handlers, so the last handler
// to finish takes it with it. Nothing here points back at the lambdas, so
// there is no cycle and nothing to clean up by hand.
// The searches to run on one Workday site: one per job title in the profile,
// up to three. With no titles, one search with no words, which brings back the
// newest jobs.
QStringList workdaySearchesFor(const JobSearchProfile &searchProfile)
{
    QStringList searches = searchProfile.targetJobTitles().mid(0, 3);
    if (searches.isEmpty()) {
        searches.append(QString());
    }
    return searches;
}

struct WatchedBoardSweep {
    JobScoutReply *scoutReply = nullptr;
    int boardsStillAnswering = 0;
    QList<JobPosting> everythingFound;
    QStringList companiesThatDidNotAnswer;
    QStringList employersReadInFull;     // "board/tenant" for each board that answered
    QSet<QString> linksAlreadyFound;     // one Workday job can match two title searches
    int companiesAsked = 0;
    bool alreadyAnswered = false;
};

void finishIfEveryBoardHasAnswered(const std::shared_ptr<WatchedBoardSweep> &sweep)
{
    if (sweep->boardsStillAnswering > 0 || sweep->alreadyAnswered) {
        return;
    }
    sweep->alreadyAnswered = true;

    // Nothing found, and at least one company never answered. Say which ones.
    // Telling somebody "no jobs" when the truth is "nobody answered" sends
    // them off deleting companies that were fine all along.
    //
    // When jobs DID come back, the jobs are the answer and this stays quiet.
    // The reply carries findings or a failure, never both, so a company that
    // was unreachable during a sweep that still found work goes unmentioned.
    // Worth knowing, and worth fixing the day a reply can carry an aside.
    if (sweep->everythingFound.isEmpty() && !sweep->companiesThatDidNotAnswer.isEmpty()) {
        const bool everyOneOfThemFailed =
            sweep->companiesThatDidNotAnswer.count() == sweep->companiesAsked;
        sweep->scoutReply->markFailed(
            (everyOneOfThemFailed
                 ? QStringLiteral("couldn't reach any of the boards you're watching — %1. "
                                  "Check your connection and try again.")
                 : QStringLiteral("found nothing, and couldn't reach %1. Check your "
                                  "connection and scout again."))
                .arg(sweep->companiesThatDidNotAnswer.join(QStringLiteral(", "))));
        return;
    }

    sweep->scoutReply->setEmployersReadInFull(sweep->employersReadInFull);
    sweep->scoutReply->markFinished(sweep->everythingFound);
}

} // namespace

FollowedEmployerJobSource::FollowedEmployerJobSource(
    const FollowedEmployerRoster &followedRoster)
    : followedRoster(followedRoster)
{
}

JobSourceDescriptor FollowedEmployerJobSource::descriptor() const
{
    bool found = false;
    return jobSourceDescriptorFor(followedEmployersStorageName, found);
}

JobScoutReply *FollowedEmployerJobSource::searchForJobs(const JobSearchProfile &searchProfile,
                                                        QObject *replyParent)
{
    // The profile is not used here on purpose. A watched company's whole board
    // is fetched, and the scorer ranks it afterwards. Filtering at the request
    // would mean deciding for the user that a job at a company they chose to
    // watch is not worth showing them, which is not this class's decision.
    // One exception: Workday. A big employer's Workday site holds thousands of
    // jobs, so it is searched for the titles in the profile instead of read
    // whole. See searchesFor below.
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    const QList<FollowedEmployer> watchedEmployers = followedRoster.allFollowedEmployers();
    if (watchedEmployers.isEmpty()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "isn't watching anyone yet. Paste a link to a job at a company you'd like "
            "to work for, in Settings, and Job Crush will read their whole board."),
            false);
        return scoutReply;
    }

    auto sweep = std::make_shared<WatchedBoardSweep>();
    sweep->scoutReply = scoutReply;
    sweep->companiesAsked = watchedEmployers.count();
    sweep->boardsStillAnswering = watchedEmployers.count();

    for (const FollowedEmployer &employer : watchedEmployers) {
        EmployerBoardProvider *boardReader = nullptr;
        if (employer.boardName == AtsBoardName::Greenhouse) boardReader = &greenhouseBoard;
        else if (employer.boardName == AtsBoardName::Lever)  boardReader = &leverBoard;
        else if (employer.boardName == AtsBoardName::Ashby)  boardReader = &ashbyBoard;
        else if (employer.boardName == AtsBoardName::SmartRecruiters) {
            boardReader = &smartRecruitersBoard;
        } else if (employer.boardName == AtsBoardName::Workday) {
            boardReader = &workdayBoard;
        }

        if (boardReader == nullptr) {
            // A company saved before Job Crush could read its board. Not an
            // error worth showing — it simply contributes nothing this sweep.
            --sweep->boardsStillAnswering;
            --sweep->companiesAsked;
            continue;
        }

        const QString companyName = employer.displayName;
        const QString employerKey =
            QStringLiteral("%1/%2").arg(employer.boardName, employer.tenant).toLower();
        const bool isWorkday = employer.boardName == AtsBoardName::Workday;

        // Workday is searched, everything else is read whole. A Workday
        // company counts as answered when ANY of its searches answers.
        const QStringList searches = isWorkday ? workdaySearchesFor(searchProfile)
                                               : QStringList { QString() };
        auto searchesStillAnswering = std::make_shared<int>(searches.count());
        auto anySearchAnswered = std::make_shared<bool>(false);

        for (const QString &searchWords : searches) {
            JobScoutReply *boardReply = isWorkday
                ? boardReader->fetchJobsLikeTitle(employer.tenant, searchWords, scoutReply)
                : boardReader->fetchEveryJobForEmployer(employer.tenant, scoutReply);

            QObject::connect(boardReply, &JobScoutReply::finished, scoutReply,
                             [sweep, boardReply, employerKey, isWorkday,
                              searchesStillAnswering, anySearchAnswered](
                                 const QList<JobPosting> &boardPostings) {
                boardReply->deleteLater();
                *anySearchAnswered = true;
                // A search never reads a whole board, so a Workday job missing
                // from it is not gone. Only whole reads count.
                if (!isWorkday && !boardReply->readOnlyPartOfTheBoard()) {
                    sweep->employersReadInFull.append(employerKey);
                }
                // These land in the "Companies you follow" tab rather than a
                // Greenhouse or Lever one, because that is the box the user
                // ticked and tabs come from ticked boxes. Which board it came
                // from is still there in the link on every row.
                for (JobPosting posting : boardPostings) {
                    if (sweep->linksAlreadyFound.contains(posting.sourceUrl)) {
                        continue;
                    }
                    sweep->linksAlreadyFound.insert(posting.sourceUrl);
                    posting.postingSource = followedEmployersStorageName;
                    sweep->everythingFound.append(posting);
                }
                if (--*searchesStillAnswering == 0) {
                    --sweep->boardsStillAnswering;
                    finishIfEveryBoardHasAnswered(sweep);
                }
            });

            QObject::connect(boardReply, &JobScoutReply::failed, scoutReply,
                             [sweep, boardReply, companyName, searchesStillAnswering,
                              anySearchAnswered](const QString &, bool) {
                boardReply->deleteLater();
                if (--*searchesStillAnswering == 0) {
                    // One company being unreachable must not sink the rest.
                    // Which one failed is remembered, so the report can name it.
                    if (!*anySearchAnswered) {
                        sweep->companiesThatDidNotAnswer.append(companyName);
                    }
                    --sweep->boardsStillAnswering;
                    finishIfEveryBoardHasAnswered(sweep);
                }
            });
        }
    }

    // Every watched company was on a board Job Crush cannot read yet, so there
    // is nothing in flight and the answer is already known.
    //
    // It still has to WAIT. Answering here would emit before searchForJobs has
    // returned, so JobScout would not have connected yet and would never hear
    // it — and a sweep that never hears its last source never ends, which
    // leaves the Scout button dead until the app is restarted.
    if (sweep->boardsStillAnswering == 0) {
        QMetaObject::invokeMethod(scoutReply, [sweep]() {
            finishIfEveryBoardHasAnswered(sweep);
        }, Qt::QueuedConnection);
    }

    return scoutReply;
}
