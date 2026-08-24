#pragma once

#include <QNetworkAccessManager>

#include "EmployerBoardProvider.h"

// GreenhouseBoardSource
//
// Reads an employer's Greenhouse job board.
//
// Greenhouse publishes this endpoint for exactly this purpose and documents
// that GET requests to it need no authentication. It is the cleanest source
// Job Crush has: the employer's own words, the full description, and a list
// that shrinks when a job is filled.
//
//   Every job:  GET https://boards-api.greenhouse.io/v1/boards/{tenant}/jobs?content=true
//   One job:    GET https://boards-api.greenhouse.io/v1/boards/{tenant}/jobs/{id}
//
// The board token is the {tenant} part of a Greenhouse URL, which
// AtsBoardDetector already pulls out of any link the user pastes.
class GreenhouseBoardSource : public EmployerBoardProvider {
public:
    GreenhouseBoardSource();

    QString boardName() const override;

    JobScoutReply *fetchEveryJobForEmployer(const QString &tenant,
                                            QObject *replyParent) override;

    JobScoutReply *fetchOneJob(const AtsBoardIdentity &boardIdentity,
                               QObject *replyParent) override;

private:
    QNetworkAccessManager networkAccessManager;
};
