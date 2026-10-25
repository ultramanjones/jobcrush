#pragma once

#include <QObject>

#include "AshbyBoardSource.h"
#include "GreenhouseBoardSource.h"
#include "JobLead.h"
#include "LeverBoardSource.h"
#include "SmartRecruitersBoardSource.h"
#include "TaleoBoardSource.h"
#include "WorkdayBoardSource.h"

class JobScoutReply;

// CanonicalPostingResolver
//
// Turns a lead into the employer's own posting.
//
// A lead is second-hand: a forwarded LinkedIn alert, a recruiter email, a
// pasted link, a summary from a search API. It may be a month stale, it may be
// a rewrite, and the job may already be filled. The employer's own board has
// none of those problems, so if Job Crush can find the same job there, that is
// the version worth keeping.
//
// Three ways in, tried in this order:
//
//   1. The link already names a job on a known hiring system. Fetch that job.
//      Nothing to guess.
//   2. The link names the employer's board but not one job. Fetch the board
//      and find the job by title.
//   3. There is no usable link — only a company and a title. Guess the board
//      account from the company name and hunt on Greenhouse, Lever, Ashby and
//      SmartRecruiters at once. When the shipped company list names the
//      employer's board, that board is searched too. That is the only way to
//      reach a Workday site, because a Workday address cannot be guessed.
//
// The third way is the LinkedIn workaround. Job Crush never fetches LinkedIn.
// It reads the company and the title out of what the user already has, then
// goes and finds the same job where it is allowed to look.
//
// The reply finishes with one posting when the job was found and none when it
// was not. Finding nothing is not a failure: plenty of employers use a hiring
// system Job Crush cannot read, and the lead is still a lead.
class CanonicalPostingResolver : public QObject {
    Q_OBJECT
public:
    explicit CanonicalPostingResolver(QObject *parent = nullptr);

    // Ownership: the reply is parented to replyParent. Callers should
    // deleteLater() it once finished or failed has fired.
    JobScoutReply *resolve(const JobLead &jobLead, QObject *replyParent);

private:
    // The reader for one hiring system, by its AtsBoardName, or nullptr when
    // Job Crush has no reader for that system.
    EmployerBoardProvider *boardReaderFor(const QString &boardName);

    // One reader per hiring system, built once and reused for every lead.

    GreenhouseBoardSource greenhouseBoard;
    LeverBoardSource leverBoard;
    AshbyBoardSource ashbyBoard;
    SmartRecruitersBoardSource smartRecruitersBoard;
    WorkdayBoardSource workdayBoard;
    TaleoBoardSource taleoBoard;
};
