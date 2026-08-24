#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "FollowedEmployer.h"

// FollowedEmployerRoster
//
// The companies the user is watching, and where they keep their job boards.
//
// A ModelView resident owning its own persistence, the same shape as
// JobSourceRoster and AiCredentialRoster. Three rosters, one pattern.
//
// The user never types a board name or an account slug. They paste a link to
// any job at the company and this works the rest out — which is the only way
// to ask for this that does not require knowing what Greenhouse is.
class FollowedEmployerRoster : public QObject {
    Q_OBJECT
public:
    explicit FollowedEmployerRoster(QObject *parent = nullptr);

    // Loads the watched companies. Called once from the composition root.
    void loadFromSettings();

    QList<FollowedEmployer> allFollowedEmployers() const;
    int followedEmployerCount() const;

    // Works the company out of a pasted link and starts watching it.
    //
    // Returns the sentence to show the user, either way. A refusal that only
    // says "no" leaves them guessing which part of the link was wrong, and
    // the link is the only thing they had to give.
    QString followEmployerFromLink(const QString &pastedLink);

    // Stops watching. Jobs already found stay where they are — they were real
    // when they arrived, and deleting somebody's list because they stopped
    // watching a company would be the app throwing away their work.
    void stopFollowing(const QString &employerKey);

signals:
    void followedEmployersChanged();

private:
    void persistToSettings() const;

    QList<FollowedEmployer> watchedEmployers;
};
