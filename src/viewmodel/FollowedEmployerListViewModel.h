#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class FollowedEmployerRoster;

// FollowedEmployerListViewModel
//
// Serves the "companies you're watching" list in Settings.
//
// Translation and organization only. Working out which hiring system a pasted
// link belongs to, and keeping the list, both happen below in the roster.
class FollowedEmployerListViewModel : public QObject {
    Q_OBJECT

    // Each entry: { employerKey, displayName, boardDisplayName }
    Q_PROPERTY(QVariantList followedEmployers READ followedEmployers
                   NOTIFY followedEmployersChanged)

    Q_PROPERTY(int followedEmployerCount READ followedEmployerCount
                   NOTIFY followedEmployersChanged)

    // What the last add or removal did, in one sentence for the user to read.
    // Always ends with something they can do next.
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY lastActionTextChanged)

public:
    explicit FollowedEmployerListViewModel(FollowedEmployerRoster &followedRoster,
                                           QObject *parent = nullptr);

    QVariantList followedEmployers() const;
    int followedEmployerCount() const;
    QString lastActionText() const;

    // Paste a link to any job at the company. Job Crush works out the rest.
    Q_INVOKABLE void followFromLink(const QString &pastedLink);

    // Stops watching. Jobs already found stay where they are.
    Q_INVOKABLE void stopFollowing(const QString &employerKey);

signals:
    void followedEmployersChanged();
    void lastActionTextChanged();

private:
    FollowedEmployerRoster &roster;
    QString storedLastActionText;
};
