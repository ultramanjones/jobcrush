#include "FollowedEmployerRoster.h"

#include <QSettings>
#include <QStringList>

#include "AtsBoardDetector.h"
#include "EmployerBoardHttp.h"

namespace {

const QString followedEmployersKey =
    QStringLiteral("jobScout/followedEmployers");

// Stored as one string per company: "board|tenant|display name". A plain list
// of strings rather than a settings group per company, because QSettings
// groups with user-supplied names in them are a way to end up with a settings
// file nobody can read by eye.
QString packOne(const FollowedEmployer &employer)
{
    return QStringLiteral("%1|%2|%3")
        .arg(employer.boardName, employer.tenant, employer.displayName);
}

FollowedEmployer unpackOne(const QString &packedText)
{
    // Split into three at most, so a display name that happens to contain a
    // pipe survives the round trip instead of being cut short.
    const QStringList parts = packedText.split(QLatin1Char('|'));
    FollowedEmployer employer;
    if (parts.count() >= 2) {
        employer.boardName = parts.at(0);
        employer.tenant = parts.at(1);
    }
    employer.displayName = parts.count() >= 3
        ? QStringList(parts.mid(2)).join(QLatin1Char('|'))
        : QString();
    if (employer.displayName.isEmpty()) {
        employer.displayName = companyNameFromTenant(employer.tenant);
    }
    return employer;
}

} // namespace

FollowedEmployerRoster::FollowedEmployerRoster(QObject *parent)
    : QObject(parent)
{
}

void FollowedEmployerRoster::loadFromSettings()
{
    QSettings settings;
    watchedEmployers.clear();
    const QStringList packedList = settings.value(followedEmployersKey).toStringList();
    for (const QString &packedText : packedList) {
        const FollowedEmployer employer = unpackOne(packedText);

        // A board name Job Crush does not recognize is dropped rather than
        // kept. Kept, it would sit in the list looking like it was being
        // watched while every sweep quietly skipped it.
        if (employer.isUsable()
                && !AtsBoardName::displayNameFor(employer.boardName).isEmpty()) {
            watchedEmployers.append(employer);
        }
    }
    emit followedEmployersChanged();
}

void FollowedEmployerRoster::persistToSettings() const
{
    QStringList packedList;
    for (const FollowedEmployer &employer : watchedEmployers) {
        packedList.append(packOne(employer));
    }
    QSettings settings;
    settings.setValue(followedEmployersKey, packedList);
    settings.sync();
}

QList<FollowedEmployer> FollowedEmployerRoster::allFollowedEmployers() const
{
    return watchedEmployers;
}

int FollowedEmployerRoster::followedEmployerCount() const
{
    return watchedEmployers.count();
}

QString FollowedEmployerRoster::followEmployerFromLink(const QString &pastedLink)
{
    const QString trimmedLink = pastedLink.trimmed();
    if (trimmedLink.isEmpty()) {
        return QStringLiteral("Paste a link to any job at the company and Job Crush will "
                              "work out where their board lives.");
    }

    AtsBoardDetector boardDetector;
    const AtsBoardIdentity boardIdentity = boardDetector.identify(trimmedLink);

    if (!boardIdentity.isKnown()) {
        if (boardDetector.isWalledGarden(trimmedLink)) {
            return QStringLiteral(
                "That's a LinkedIn or Indeed link, and Job Crush isn't allowed to read "
                "those. Open the job on the company's own careers page and paste the "
                "link from there.");
        }
        return QStringLiteral(
            "Job Crush can watch companies on Greenhouse, Lever and Ashby. That link "
            "isn't one of them. Open a job on the company's careers page — if the "
            "address says greenhouse, lever or ashby anywhere in it, that link works.");
    }

    const QString boardDisplayName = AtsBoardName::displayNameFor(boardIdentity.boardName);
    if (boardDisplayName != QStringLiteral("Greenhouse")
            && boardDisplayName != QStringLiteral("Lever")
            && boardDisplayName != QStringLiteral("Ashby")) {
        return QStringLiteral(
            "That company is on %1, and Job Crush can't read %1 yet — only Greenhouse, "
            "Lever and Ashby. It's on the list.").arg(boardDisplayName);
    }

    FollowedEmployer employer;
    employer.boardName = boardIdentity.boardName;
    employer.tenant = boardIdentity.tenant;
    employer.displayName = companyNameFromTenant(boardIdentity.tenant);

    for (const FollowedEmployer &alreadyWatched : watchedEmployers) {
        if (alreadyWatched.asKey().compare(employer.asKey(), Qt::CaseInsensitive) == 0) {
            return QStringLiteral("You're already watching %1. Their jobs turn up every "
                                  "time you scout.").arg(alreadyWatched.displayName);
        }
    }

    watchedEmployers.append(employer);
    persistToSettings();
    emit followedEmployersChanged();

    return QStringLiteral("Now watching %1 on %2. Their whole board turns up next time "
                          "you scout.").arg(employer.displayName, boardDisplayName);
}

void FollowedEmployerRoster::stopFollowing(const QString &employerKey)
{
    const int countBefore = watchedEmployers.count();
    for (int watchedIndex = watchedEmployers.count() - 1; watchedIndex >= 0; --watchedIndex) {
        // Case-insensitive, the same way followEmployerFromLink decides two
        // entries are the same company. Matching one way when adding and
        // another when removing means a row that cannot be removed.
        if (watchedEmployers.at(watchedIndex).asKey()
                .compare(employerKey, Qt::CaseInsensitive) == 0) {
            watchedEmployers.removeAt(watchedIndex);
        }
    }
    if (watchedEmployers.count() == countBefore) {
        return;
    }
    persistToSettings();
    emit followedEmployersChanged();
}
