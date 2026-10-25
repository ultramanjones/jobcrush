#pragma once

#include "AshbyBoardSource.h"
#include "GreenhouseBoardSource.h"
#include "JobSourceProvider.h"
#include "LeverBoardSource.h"
#include "SmartRecruitersBoardSource.h"
#include "TaleoBoardSource.h"
#include "WorkdayBoardSource.h"

class FollowedEmployerRoster;

// FollowedEmployerJobSource
//
// Reads every board the user is watching, and hands the lot back as one
// source.
//
// It looks like a job site to everything above it — same interface, one tab,
// one line in the sweep report — but there is no site. It is however many
// companies the user chose, each read from its own hiring system.
//
// It adds no new network code. The board readers were already written for
// the resolver, which only ever asked them for one job at a time. Asked for
// the whole board instead, the same code answers "what is Acme hiring for
// right now?" — the question a person with a list of places they want to
// work is asking.
//
// It holds one reader per board it can read. Each company on the roster is
// sent to the reader for its board.
class FollowedEmployerJobSource : public JobSourceProvider {
public:
    explicit FollowedEmployerJobSource(const FollowedEmployerRoster &followedRoster);

    JobSourceDescriptor descriptor() const override;

    JobScoutReply *searchForJobs(const JobSearchProfile &searchProfile,
                                 QObject *replyParent) override;

private:
    const FollowedEmployerRoster &followedRoster;

    GreenhouseBoardSource greenhouseBoard;
    LeverBoardSource leverBoard;
    AshbyBoardSource ashbyBoard;
    SmartRecruitersBoardSource smartRecruitersBoard;
    WorkdayBoardSource workdayBoard;
    TaleoBoardSource taleoBoard;
};
