#pragma once

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

#include "../jobscout/AshbyBoardSource.h"
#include "../jobscout/AtsBoardIdentity.h"
#include "../jobscout/GreenhouseBoardSource.h"
#include "../jobscout/LeverBoardSource.h"
#include "../jobscout/SmartRecruitersBoardSource.h"
#include "../jobscout/TaleoBoardSource.h"
#include "../jobscout/WorkdayBoardSource.h"

// What the company's own board said about one job.
enum class ListingAnswer {
    StillListed,    // the board still lists it
    Gone,           // two separate questions to the board both said it is not there
    CouldNotCheck   // the board did not answer, or the two answers did not agree
};

Q_DECLARE_METATYPE(ListingAnswer)

// ListingCheckReply
//
// One check of one job, still running. The same pattern as JobScoutReply:
// ask, get one of these back, and connect to answered. It deletes itself
// after it answers.
class ListingCheckReply : public QObject {
    Q_OBJECT
public:
    explicit ListingCheckReply(QObject *parent = nullptr) : QObject(parent) {}

    // Called by PostingGoneConfirmer. Not for consumers.
    void answer(ListingAnswer listingAnswer, const QString &whatHappened)
    {
        emit answered(listingAnswer, whatHappened);
        deleteLater();
    }

signals:
    // whatHappened is a plain sentence for the log, such as "Workday said
    // the job page was not found, and a search of the board did not find the
    // title either."
    void answered(ListingAnswer listingAnswer, const QString &whatHappened);
};

// PostingGoneConfirmer
//
// Decides whether a job is really gone from the company's own board. Used by
// the "Are these still open?" check and by the sweep of the companies the
// user follows.
//
// A job is called gone only when two separate answers from the company's
// board agree:
//  1. The job's own page on the board says "not found".
//  2. The company's list of jobs, or a search of it for the job's title,
//     does not have the job either: not by its id, and not by its title.
//     On Workday the board is also searched for the job's requisition
//     number. A list cut short at the page limit does not count.
//
// If the first answer is "not found" but the job, or a job with the same
// title, is still in the company's list, the job is still listed. If either
// question cannot be answered, the answer is "could not check". Job Crush
// never says a job is gone on one answer alone, because a wrong "gone"
// can make the user drop a job that is still open.
class PostingGoneConfirmer {
public:
    // True when Job Crush can check jobs on this board.
    bool canCheckBoard(const QString &boardName) const;

    // Checks one job. identity is the job's link on the board, read by
    // AtsBoardDetector. positionTitle is used for the second question.
    ListingCheckReply *checkOneJob(const AtsBoardIdentity &identity,
                                   const QString &positionTitle,
                                   QObject *replyParent);

    // Uses a different reader for one board. The tests use this to answer
    // for a board without the network. The app never calls it.
    void replaceBoardReader(const QString &boardName, EmployerBoardProvider *boardReader);

private:
    EmployerBoardProvider *boardReaderFor(const QString &boardName);

    // The second question: does the company's board still list this job, by
    // its id or by its title?
    void askTheCompanyBoard(EmployerBoardProvider *boardReader,
                            const AtsBoardIdentity &identity,
                            const QString &positionTitle,
                            ListingCheckReply *checkReply);

    // Runs the searches one at a time. Stops at the first that lists the
    // job. Says gone only when every search answered in full without it.
    void searchTheCompanyBoard(EmployerBoardProvider *boardReader,
                               const AtsBoardIdentity &identity,
                               const QString &wantedComparableTitle,
                               const QStringList &searchesLeft,
                               ListingCheckReply *checkReply);

    // Readers the tests put in place of the real ones, by board name. Not
    // owned.
    QHash<QString, EmployerBoardProvider *> replacementBoardReaders;

    // The real readers, one per board Job Crush can check.
    GreenhouseBoardSource greenhouseBoard;
    LeverBoardSource leverBoard;
    AshbyBoardSource ashbyBoard;
    WorkdayBoardSource workdayBoard;
    SmartRecruitersBoardSource smartRecruitersBoard;
    TaleoBoardSource taleoBoard;
};
