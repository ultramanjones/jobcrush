#pragma once

#include <QNetworkAccessManager>

#include "EmployerBoardProvider.h"

// AshbyBoardSource
//
// Reads an employer's Ashby job board.
//
//   Every job:  GET https://api.ashbyhq.com/posting-api/job-board/{tenant}?includeCompensation=true
//
// No key, no sign-up, same as Greenhouse and Lever.
//
// Ashby has no endpoint for one job. There is only the whole board. So asking
// for a single job fetches the board and picks that job out of it. That costs
// one extra response and is the only way Ashby offers, and it has an upside:
// a job missing from the board has been closed, which is an answer worth
// having.
//
// Ashby does not send the company's real name either, so the board account
// name stands in for it, same as Lever.
class AshbyBoardSource : public EmployerBoardProvider {
public:
    AshbyBoardSource();

    QString boardName() const override;

    JobScoutReply *fetchEveryJobForEmployer(const QString &tenant,
                                            QObject *replyParent) override;

    JobScoutReply *fetchOneJob(const AtsBoardIdentity &boardIdentity,
                               QObject *replyParent) override;

private:
    QNetworkAccessManager networkAccessManager;

    // Both public calls fetch the same board. wantedJobId empty means "keep
    // every job"; set, it means "keep only that one".
    JobScoutReply *fetchBoardAndKeep(const QString &tenant,
                                     const QString &wantedJobId,
                                     QObject *replyParent);
};
