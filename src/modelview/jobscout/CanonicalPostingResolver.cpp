#include "CanonicalPostingResolver.h"

#include <memory>

#include <QDebug>
#include <QList>
#include <QStringList>

#include "AtsBoardDetector.h"
#include "DuplicateJobDetector.h"
#include "EmployerBoardHttp.h"
#include "EmployerTenantGuesser.h"
#include "JobScoutReply.h"

namespace {

// The lead as a posting, so the same comparison code works on both.
JobPosting postingShapeOf(const JobLead &jobLead)
{
    JobPosting leadAsPosting;
    leadAsPosting.companyName = jobLead.companyName;
    leadAsPosting.positionTitle = jobLead.positionTitle;
    leadAsPosting.locationText = jobLead.locationText;
    leadAsPosting.isRemoteRole = jobLead.isRemoteRole;
    leadAsPosting.sourceUrl = jobLead.discoveryUrl;
    leadAsPosting.postingSource = jobLead.postingSource;
    return leadAsPosting;
}

// Is this posting off the board the same job the lead describes?
//
// The company is NOT checked here, and that is on purpose. By the time this
// runs, Job Crush is already reading one specific employer's board — either
// because the link named it or because the board account was guessed from the
// company name. Checking the company again would only compare a name against
// itself. The title has to match exactly once the search-engine noise is
// stripped, and the places must not contradict.
bool sameJobAsTheLead(const JobPosting &boardPosting, const JobPosting &leadAsPosting)
{
    const QString boardTitle = DuplicateJobDetector::plainTitle(boardPosting.positionTitle);
    const QString leadTitle = DuplicateJobDetector::plainTitle(leadAsPosting.positionTitle);
    if (boardTitle.isEmpty() || boardTitle != leadTitle) {
        return false;
    }
    return DuplicateJobDetector::locationsAgree(boardPosting, leadAsPosting);
}

const JobPosting *findTheLeadIn(const QList<JobPosting> &boardPostings,
                                const JobPosting &leadAsPosting)
{
    for (const JobPosting &boardPosting : boardPostings) {
        if (sameJobAsTheLead(boardPosting, leadAsPosting)) {
            return &boardPosting;
        }
    }
    return nullptr;
}

// One board being searched, and which account names are left to try on it.
struct BoardBeingSearched {
    EmployerBoardProvider *boardReader = nullptr;
    QStringList tenantsLeftToTry;
    QString tenantBeingTried;
    bool givenUp = false;
};

// The state of one hunt across all three boards at once.
//
// Held by shared_ptr and captured by the reply handlers. When the last handler
// is gone, so is this. Nothing here points back at the lambdas, so there is no
// cycle and nothing to clean up by hand.
struct EmployerBoardHunt {
    JobScoutReply *scoutReply = nullptr;
    JobPosting leadAsPosting;
    QList<BoardBeingSearched> boards;
    bool alreadyAnswered = false;

    // Set when a board could not be reached at all, as opposed to answering
    // "no board by that name". If nothing is found and this is set, the honest
    // report is "couldn't check", not "not there".
    bool someBoardCouldNotBeReached = false;
    QString firstTroubleSeen;

    bool everyBoardHasGivenUp() const
    {
        for (const BoardBeingSearched &board : boards) {
            if (!board.givenUp) {
                return false;
            }
        }
        return true;
    }
};

void tryTheNextAccountName(const std::shared_ptr<EmployerBoardHunt> &hunt, int boardIndex);

// Some boards do not send the company's real name. Workday sends an internal
// office name instead ("0090 CORP-Corporate Office"), so the name the user
// gave is better than anything the board says.
JobPosting withTheLeadsCompanyNameIfTheBoardHasNone(JobPosting boardPosting,
                                                    const JobPosting &leadAsPosting)
{
    if (boardPosting.postingSource == AtsBoardName::Workday
            && !leadAsPosting.companyName.trimmed().isEmpty()) {
        boardPosting.companyName = leadAsPosting.companyName.trimmed();
    }
    return boardPosting;
}

// The job was found on a board. Some boards (Workday) find jobs with a search
// that sends no description, so the full posting is fetched before answering.
// The user should get the employer's whole posting, not a title and a link.
void answerWithTheFoundPosting(const std::shared_ptr<EmployerBoardHunt> &hunt,
                               int boardIndex,
                               const JobPosting &foundPosting)
{
    hunt->alreadyAnswered = true;
    const BoardBeingSearched &board = hunt->boards.at(boardIndex);

    if (!foundPosting.fullDescriptionText.trimmed().isEmpty()
            || foundPosting.externalSourceId.isEmpty()) {
        hunt->scoutReply->markFinished(
            { withTheLeadsCompanyNameIfTheBoardHasNone(foundPosting, hunt->leadAsPosting) });
        return;
    }

    AtsBoardIdentity jobIdentity;
    jobIdentity.boardName = board.boardReader->boardName();
    jobIdentity.tenant = board.tenantBeingTried;
    jobIdentity.jobId = foundPosting.externalSourceId;

    JobScoutReply *oneJobReply = board.boardReader->fetchOneJob(jobIdentity, hunt->scoutReply);
    QObject::connect(oneJobReply, &JobScoutReply::finished, hunt->scoutReply,
                     [hunt, oneJobReply, foundPosting](const QList<JobPosting> &fullPostings) {
        oneJobReply->deleteLater();
        // If the full posting did not come back, the search row is still the
        // right job. Keep it rather than throwing the find away.
        const JobPosting bestPosting = fullPostings.isEmpty() ? foundPosting
                                                              : fullPostings.first();
        hunt->scoutReply->markFinished(
            { withTheLeadsCompanyNameIfTheBoardHasNone(bestPosting, hunt->leadAsPosting) });
    });
    QObject::connect(oneJobReply, &JobScoutReply::failed, hunt->scoutReply,
                     [hunt, oneJobReply, foundPosting](const QString &whyItFailed) {
        oneJobReply->deleteLater();
        qWarning().noquote() << QStringLiteral(
            "CanonicalPostingResolver::answerWithTheFoundPosting: full posting for \"%1\" "
            "did not come back, keeping the search row — %2")
            .arg(foundPosting.positionTitle, whyItFailed);
        hunt->scoutReply->markFinished(
            { withTheLeadsCompanyNameIfTheBoardHasNone(foundPosting, hunt->leadAsPosting) });
    });
}

// Every board has given up. Whether that means "not there" or "couldn't
// check" depends on what went wrong along the way, and the difference matters
// to whoever reads the message.
void answerThatNothingWasFound(const std::shared_ptr<EmployerBoardHunt> &hunt)
{
    if (hunt->alreadyAnswered) {
        return;
    }
    hunt->alreadyAnswered = true;

    if (hunt->someBoardCouldNotBeReached) {
        hunt->scoutReply->markFailed(
            QStringLiteral("Job Crush couldn't reach the job boards to check — %1")
                .arg(hunt->firstTroubleSeen));
        return;
    }
    hunt->scoutReply->markFinished({});
}

// One board answered. Decide what that means and what to do next.
void handleOneBoardAnswer(const std::shared_ptr<EmployerBoardHunt> &hunt,
                          int boardIndex,
                          const QList<JobPosting> &boardPostings)
{
    if (hunt->alreadyAnswered) {
        return;
    }

    if (const JobPosting *foundPosting = findTheLeadIn(boardPostings, hunt->leadAsPosting)) {
        answerWithTheFoundPosting(hunt, boardIndex, *foundPosting);
        return;
    }

    BoardBeingSearched &board = hunt->boards[boardIndex];

    // Jobs came back, so this account name was right — this really is the
    // employer's board. The job just is not on it, which usually means it has
    // been filled. Trying more spellings of a name that already worked would
    // only find other companies, so this board is done.
    if (!boardPostings.isEmpty()) {
        board.givenUp = true;
    }

    if (board.givenUp || board.tenantsLeftToTry.isEmpty()) {
        board.givenUp = true;
        if (hunt->everyBoardHasGivenUp()) {
            answerThatNothingWasFound(hunt);
        }
        return;
    }

    tryTheNextAccountName(hunt, boardIndex);
}

void tryTheNextAccountName(const std::shared_ptr<EmployerBoardHunt> &hunt, int boardIndex)
{
    BoardBeingSearched &board = hunt->boards[boardIndex];
    if (board.tenantsLeftToTry.isEmpty()) {
        board.givenUp = true;
        if (hunt->everyBoardHasGivenUp()) {
            answerThatNothingWasFound(hunt);
        }
        return;
    }

    const QString tenantToTry = board.tenantsLeftToTry.takeFirst();
    board.tenantBeingTried = tenantToTry;
    JobScoutReply *boardReply = board.boardReader->fetchJobsLikeTitle(
        tenantToTry, hunt->leadAsPosting.positionTitle, hunt->scoutReply);

    QObject::connect(boardReply, &JobScoutReply::finished, hunt->scoutReply,
                     [hunt, boardIndex, boardReply](const QList<JobPosting> &boardPostings) {
        boardReply->deleteLater();
        handleOneBoardAnswer(hunt, boardIndex, boardPostings);
    });

    // A board that says "no account by that name" is not a problem worth
    // showing anyone — with guessed names that is the expected answer most of
    // the time, and it means the same thing as an empty board: keep going.
    //
    // A board that could not be REACHED is different, and it is remembered.
    // Reporting "not on Greenhouse" when the truth is the connection dropped
    // sends the user off solving a problem they do not have.
    QObject::connect(boardReply, &JobScoutReply::failed, hunt->scoutReply,
                     [hunt, boardIndex, boardReply](const QString &whyItFailed,
                                                    bool sourceHadTrouble) {
        boardReply->deleteLater();
        if (sourceHadTrouble) {
            hunt->someBoardCouldNotBeReached = true;
            if (hunt->firstTroubleSeen.isEmpty()) {
                hunt->firstTroubleSeen = whyItFailed;
            }
        }
        handleOneBoardAnswer(hunt, boardIndex, {});
    });
}

} // namespace

CanonicalPostingResolver::CanonicalPostingResolver(QObject *parent)
    : QObject(parent)
{
}

EmployerBoardProvider *CanonicalPostingResolver::boardReaderFor(const QString &boardName)
{
    if (boardName == AtsBoardName::Greenhouse) return &greenhouseBoard;
    if (boardName == AtsBoardName::Lever)      return &leverBoard;
    if (boardName == AtsBoardName::Ashby)      return &ashbyBoard;
    if (boardName == AtsBoardName::Workday)    return &workdayBoard;
    return nullptr;
}

JobScoutReply *CanonicalPostingResolver::resolve(const JobLead &jobLead, QObject *replyParent)
{
    JobScoutReply *scoutReply = new JobScoutReply(replyParent);

    if (!jobLead.hasSomethingToSearchOn()) {
        failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
            "There isn't enough here to go looking. Paste the job's link, or type the "
            "company and the job title, and Job Crush will find the real posting."));
        return scoutReply;
    }

    // The lead may already know which board it belongs to. If not, read the
    // link — a Greenhouse or Lever link says so plainly.
    AtsBoardIdentity boardIdentity = jobLead.boardIdentity;
    if (!boardIdentity.isKnown()) {
        AtsBoardDetector boardDetector;
        boardIdentity = boardDetector.identify(jobLead.discoveryUrl);
    }

    const JobPosting leadAsPosting = postingShapeOf(jobLead);

    // Way 1: the link names one job on a board Job Crush can read.
    if (boardIdentity.namesOneJob()) {
        if (EmployerBoardProvider *boardReader = boardReaderFor(boardIdentity.boardName)) {
            JobScoutReply *boardReply = boardReader->fetchOneJob(boardIdentity, scoutReply);
            QObject::connect(boardReply, &JobScoutReply::finished, scoutReply,
                             [scoutReply, boardReply, leadAsPosting](
                                 const QList<JobPosting> &boardPostings) {
                boardReply->deleteLater();
                QList<JobPosting> namedPostings;
                for (const JobPosting &boardPosting : boardPostings) {
                    namedPostings.append(
                        withTheLeadsCompanyNameIfTheBoardHasNone(boardPosting, leadAsPosting));
                }
                scoutReply->markFinished(namedPostings);
            });
            QObject::connect(boardReply, &JobScoutReply::failed, scoutReply,
                             [scoutReply, boardReply](const QString &whyItFailed,
                                                      bool sourceHadTrouble) {
                boardReply->deleteLater();
                scoutReply->markFailed(whyItFailed, sourceHadTrouble);
            });
            return scoutReply;
        }
    }

    // Way 2 and way 3 are the same search with different account names to try:
    // either the one the link handed over, or the ones guessed from the
    // company name.
    auto hunt = std::make_shared<EmployerBoardHunt>();
    hunt->scoutReply = scoutReply;
    hunt->leadAsPosting = leadAsPosting;

    if (boardIdentity.isKnown() && boardReaderFor(boardIdentity.boardName)) {
        // Way 2: the link named the employer's board. One name, one board, no
        // guessing.
        //
        // But a board is a list, and picking one job off a list means knowing
        // which job. Without a title there is nothing to match on, so the
        // search would fetch the whole board, compare every posting against an
        // empty title, match none of them, and report "not found" about a
        // board it read successfully. Say what actually happened instead.
        if (jobLead.positionTitle.trimmed().isEmpty()) {
            failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
                "That link points at the company's whole job board, not one job. Open "
                "the job you want and paste that link — or watch the company in "
                "Settings and Job Crush will read their whole board every time you "
                "scout."), false);
            return scoutReply;
        }

        BoardBeingSearched board;
        board.boardReader = boardReaderFor(boardIdentity.boardName);
        board.tenantsLeftToTry = { boardIdentity.tenant };
        hunt->boards.append(board);
    } else {
        // Way 3: nothing but a company name to go on. Try all three systems.
        const QStringList tenantGuesses =
            EmployerTenantGuesser::tenantGuessesFor(jobLead.companyName);
        if (tenantGuesses.isEmpty() || jobLead.positionTitle.trimmed().isEmpty()) {
            failThisReplyOnceTheCallerIsListening(scoutReply, QStringLiteral(
                "Job Crush needs the company's name and the job title to find the real "
                "posting. Add both and try again."));
            return scoutReply;
        }
        for (EmployerBoardProvider *boardReader :
                 { static_cast<EmployerBoardProvider *>(&greenhouseBoard),
                   static_cast<EmployerBoardProvider *>(&leverBoard),
                   static_cast<EmployerBoardProvider *>(&ashbyBoard) }) {
            BoardBeingSearched board;
            board.boardReader = boardReader;
            board.tenantsLeftToTry = tenantGuesses;
            hunt->boards.append(board);
        }
    }

    // All boards start at once. Whichever finds the job first wins, and the
    // rest stop mattering.
    for (int boardIndex = 0; boardIndex < hunt->boards.count(); ++boardIndex) {
        tryTheNextAccountName(hunt, boardIndex);
    }

    return scoutReply;
}
