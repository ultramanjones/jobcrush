#pragma once

#include <QString>

#include "AtsBoardIdentity.h"

// FollowedEmployer
//
// A company the user wants watched.
//
// Job sites answer "what jobs like this exist?". This answers a different
// question: "what is Acme hiring for right now?" — and for anyone with a list
// of places they actually want to work, that is the better question. The
// answer comes from the employer's own board, so it is the real posting, it is
// complete, and it disappears when the job is filled.
//
// Pure data.
struct FollowedEmployer {
    QString boardName;    // one of AtsBoardName
    QString tenant;       // the employer's account on that board
    QString displayName;  // what the user reads

    bool isUsable() const { return !boardName.isEmpty() && !tenant.isEmpty(); }

    // "greenhouse/acme" — how one followed employer is told from another.
    QString asKey() const
    {
        return isUsable() ? QStringLiteral("%1/%2").arg(boardName, tenant) : QString();
    }
};
