#pragma once

#include <QNetworkAccessManager>

#include "EmployerBoardProvider.h"

// LeverBoardSource
//
// Reads an employer's Lever job board.
//
// Lever publishes every posting on a public URL with no key and no sign-up,
// the same deal Greenhouse offers.
//
//   Every job:  GET https://api.lever.co/v0/postings/{tenant}?mode=json
//   One job:    GET https://api.lever.co/v0/postings/{tenant}/{jobId}?mode=json
//
// The tenant is the company part of a jobs.lever.co link, which
// AtsBoardDetector already pulls out of any link the user pastes.
//
// One thing Lever does not give: the company's real name. Greenhouse sends it;
// Lever does not, anywhere in the posting. So the tenant is the best name
// available, and it is usually right — "netflix", "figma". When it is a slug
// the user can fix it on the card.
class LeverBoardSource : public EmployerBoardProvider {
public:
    LeverBoardSource();

    QString boardName() const override;

    JobScoutReply *fetchEveryJobForEmployer(const QString &tenant,
                                            QObject *replyParent) override;

    JobScoutReply *fetchOneJob(const AtsBoardIdentity &boardIdentity,
                               QObject *replyParent) override;

private:
    QNetworkAccessManager networkAccessManager;
};
