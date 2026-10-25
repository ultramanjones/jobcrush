#include "PostingGoneConfirmer.h"

#include <QList>

#include <algorithm>

#include "../../model/JobPosting.h"
#include "../jobscout/AtsBoardDetector.h"
#include "../jobscout/JobScoutReply.h"

namespace {

// A title with only its letters and numbers, lowercase, so "Qt Developer -
// Remote" and "Qt developer (remote)" count as the same title.
QString comparableTitle(const QString &positionTitle)
{
    QString titleText = positionTitle.toLower();
    titleText.removeIf([](QChar character) {
        return !character.isLetterOrNumber();
    });
    return titleText;
}

// The requisition number at the end of a Workday job path, such as
// "R10252946" in ".../Principal-Software-Engineer_R10252946". Workday keeps
// this number when a job's location or title text in the link changes, so it
// is a better match than the whole path. Empty when there is none.
//
// The steps: take the text after the last "/", then the text after the last
// "_" in that. It counts as a requisition number only when there was an "_"
// and the text holds at least one digit, so a plain title is never taken
// for one.
QString workdayRequisitionOf(const QString &workdayJobPath)
{
    const QString lastPathPart = workdayJobPath.section(QLatin1Char('/'), -1);
    const QString afterLastUnderscore = lastPathPart.section(QLatin1Char('_'), -1);
    const bool looksLikeAnId = afterLastUnderscore != lastPathPart
        && std::any_of(afterLastUnderscore.cbegin(), afterLastUnderscore.cend(),
                       [](QChar character) { return character.isDigit(); });
    return looksLikeAnId ? afterLastUnderscore : QString();
}

// True when a posting from the company's list is the job being checked: the
// same job id, the same Workday requisition number, or the same title.
bool isTheSameJob(const JobPosting &listedPosting, const AtsBoardIdentity &identity,
                  const QString &wantedComparableTitle)
{
    const AtsBoardIdentity listedIdentity = AtsBoardDetector().identify(listedPosting.sourceUrl);
    if (!listedIdentity.jobId.isEmpty()
            && listedIdentity.jobId.compare(identity.jobId, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (identity.boardName == AtsBoardName::Workday) {
        const QString wantedRequisition = workdayRequisitionOf(identity.jobId);
        if (!wantedRequisition.isEmpty()
                && workdayRequisitionOf(listedIdentity.jobId)
                       .compare(wantedRequisition, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return !wantedComparableTitle.isEmpty()
           && comparableTitle(listedPosting.positionTitle) == wantedComparableTitle;
}

} // namespace

bool PostingGoneConfirmer::canCheckBoard(const QString &boardName) const
{
    return boardName == AtsBoardName::Greenhouse
        || boardName == AtsBoardName::Lever
        || boardName == AtsBoardName::Ashby
        || boardName == AtsBoardName::Workday
        || boardName == AtsBoardName::SmartRecruiters
        || boardName == AtsBoardName::Taleo;
}

void PostingGoneConfirmer::replaceBoardReader(const QString &boardName,
                                              EmployerBoardProvider *boardReader)
{
    replacementBoardReaders.insert(boardName, boardReader);
}

EmployerBoardProvider *PostingGoneConfirmer::boardReaderFor(const QString &boardName)
{
    if (replacementBoardReaders.contains(boardName)) {
        return replacementBoardReaders.value(boardName);
    }
    if (boardName == AtsBoardName::Greenhouse)      return &greenhouseBoard;
    if (boardName == AtsBoardName::Lever)           return &leverBoard;
    if (boardName == AtsBoardName::Ashby)           return &ashbyBoard;
    if (boardName == AtsBoardName::Workday)         return &workdayBoard;
    if (boardName == AtsBoardName::SmartRecruiters) return &smartRecruitersBoard;
    if (boardName == AtsBoardName::Taleo)           return &taleoBoard;
    return nullptr;
}

ListingCheckReply *PostingGoneConfirmer::checkOneJob(const AtsBoardIdentity &identity,
                                                      const QString &positionTitle,
                                                      QObject *replyParent)
{
    ListingCheckReply *checkReply = new ListingCheckReply(replyParent);
    EmployerBoardProvider *boardReader = boardReaderFor(identity.boardName);

    if (boardReader == nullptr || !identity.namesOneJob()) {
        // Answered on the next pass of the event loop, so the caller has
        // time to connect to the reply first.
        QMetaObject::invokeMethod(checkReply, [checkReply]() {
            checkReply->answer(ListingAnswer::CouldNotCheck,
                               QStringLiteral("Job Crush can't read that board."));
        }, Qt::QueuedConnection);
        return checkReply;
    }

    // The first question: the job's own page on the board.
    JobScoutReply *jobPageReply = boardReader->fetchOneJob(identity, checkReply);
    QObject::connect(jobPageReply, &JobScoutReply::finished, checkReply,
                     [this, boardReader, identity, positionTitle, checkReply, jobPageReply](
                         const QList<JobPosting> &foundPostings) {
        jobPageReply->deleteLater();
        if (!foundPostings.isEmpty()) {
            checkReply->answer(ListingAnswer::StillListed,
                               QStringLiteral("The job's page on the board is there."));
            return;
        }
        // The page says "not found". One answer is not enough to call the
        // job gone, so ask the company's board too.
        askTheCompanyBoard(boardReader, identity, positionTitle, checkReply);
    });
    QObject::connect(jobPageReply, &JobScoutReply::failed, checkReply,
                     [checkReply, jobPageReply](const QString &whyItFailed) {
        jobPageReply->deleteLater();
        checkReply->answer(ListingAnswer::CouldNotCheck,
                           QStringLiteral("The job's page could not be read: %1")
                               .arg(whyItFailed));
    });
    return checkReply;
}

void PostingGoneConfirmer::askTheCompanyBoard(EmployerBoardProvider *boardReader,
                                              const AtsBoardIdentity &identity,
                                              const QString &positionTitle,
                                              ListingCheckReply *checkReply)
{
    // For most boards this reads the company's whole list of jobs. Workday
    // and Taleo can have thousands, so they are searched for the job's title.
    //
    // On Workday the job's link holds its title. If the employer edits the
    // title, the old link says "not found" and a search for the old title
    // misses the job. So Workday is also searched for the job's requisition
    // number, which does not change.
    QStringList searches { positionTitle };
    if (identity.boardName == AtsBoardName::Workday) {
        const QString requisitionNumber = workdayRequisitionOf(identity.jobId);
        if (!requisitionNumber.isEmpty()) {
            searches.append(requisitionNumber);
        }
    }
    searchTheCompanyBoard(boardReader, identity, comparableTitle(positionTitle), searches,
                          checkReply);
}

void PostingGoneConfirmer::searchTheCompanyBoard(EmployerBoardProvider *boardReader,
                                                 const AtsBoardIdentity &identity,
                                                 const QString &wantedComparableTitle,
                                                 const QStringList &searchesLeft,
                                                 ListingCheckReply *checkReply)
{
    if (searchesLeft.isEmpty()) {
        checkReply->answer(ListingAnswer::Gone, QStringLiteral(
            "The job's page said not found, and the company's board does not list it "
            "by its id or its title."));
        return;
    }

    JobScoutReply *boardListReply =
        boardReader->fetchJobsLikeTitle(identity.tenant, searchesLeft.first(), checkReply);

    QObject::connect(boardListReply, &JobScoutReply::finished, checkReply,
                     [this, boardReader, identity, wantedComparableTitle, searchesLeft,
                      checkReply, boardListReply](const QList<JobPosting> &listedPostings) {
        boardListReply->deleteLater();
        for (const JobPosting &listedPosting : listedPostings) {
            if (isTheSameJob(listedPosting, identity, wantedComparableTitle)) {
                checkReply->answer(ListingAnswer::StillListed, QStringLiteral(
                    "The job's page said not found, but the company's board still "
                    "lists it."));
                return;
            }
        }
        // The reader stopped at its page limit with more jobs left to read,
        // so the job may be past the stop. That is not a "gone".
        if (boardListReply->stoppedAtThePageLimit()) {
            checkReply->answer(ListingAnswer::CouldNotCheck, QStringLiteral(
                "The job's page said not found, but the company's board has more jobs "
                "than Job Crush reads in one check, so it can't be sure."));
            return;
        }
        searchTheCompanyBoard(boardReader, identity, wantedComparableTitle,
                              searchesLeft.mid(1), checkReply);
    });
    QObject::connect(boardListReply, &JobScoutReply::failed, checkReply,
                     [checkReply, boardListReply](const QString &whyItFailed) {
        boardListReply->deleteLater();
        checkReply->answer(ListingAnswer::CouldNotCheck, QStringLiteral(
            "The job's page said not found, but the company's board could not be read "
            "to make sure: %1").arg(whyItFailed));
    });
}
