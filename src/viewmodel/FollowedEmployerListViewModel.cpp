#include "FollowedEmployerListViewModel.h"

#include <QVariantMap>

#include "../modelview/jobscout/FollowedEmployer.h"
#include "../modelview/jobscout/FollowedEmployerRoster.h"

FollowedEmployerListViewModel::FollowedEmployerListViewModel(
    FollowedEmployerRoster &followedRoster, QObject *parent)
    : QObject(parent)
    , roster(followedRoster)
{
    connect(&roster, &FollowedEmployerRoster::followedEmployersChanged,
            this, &FollowedEmployerListViewModel::followedEmployersChanged);
}

QVariantList FollowedEmployerListViewModel::followedEmployers() const
{
    QVariantList employerRows;
    for (const FollowedEmployer &employer : roster.allFollowedEmployers()) {
        QVariantMap employerRow;
        employerRow.insert(QStringLiteral("employerKey"), employer.asKey());
        employerRow.insert(QStringLiteral("displayName"), employer.displayName);
        employerRow.insert(QStringLiteral("boardDisplayName"),
                           AtsBoardName::displayNameFor(employer.boardName));
        employerRows.append(employerRow);
    }
    return employerRows;
}

int FollowedEmployerListViewModel::followedEmployerCount() const
{
    return roster.followedEmployerCount();
}

QString FollowedEmployerListViewModel::lastActionText() const
{
    return storedLastActionText;
}

void FollowedEmployerListViewModel::followFromLink(const QString &pastedLink)
{
    storedLastActionText = roster.followEmployerFromLink(pastedLink);
    emit lastActionTextChanged();
}

void FollowedEmployerListViewModel::stopFollowing(const QString &employerKey)
{
    QString employerName;
    for (const FollowedEmployer &employer : roster.allFollowedEmployers()) {
        if (employer.asKey() == employerKey) {
            employerName = employer.displayName;
            break;
        }
    }

    roster.stopFollowing(employerKey);

    storedLastActionText = employerName.isEmpty()
        ? QStringLiteral("Stopped watching that one. The jobs it already found are still "
                         "in Discoveries.")
        : QStringLiteral("Stopped watching %1. The jobs it already found are still in "
                         "Discoveries.").arg(employerName);
    emit lastActionTextChanged();
}
