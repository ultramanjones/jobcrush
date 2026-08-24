#pragma once

#include "AshbyBoardSource.h"
#include "GreenhouseBoardSource.h"
#include "JobSourceProvider.h"
#include "LeverBoardSource.h"

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
// This is the best source Job Crush has, and it costs nothing extra: the
// board readers were already written for the resolver, which only ever asked
// them for one job at a time. Asked for the whole board instead, the same code
// answers "what is Acme hiring for right now?" — which, for anyone with a list
// of places they want to work, is the question that matters.
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
};
