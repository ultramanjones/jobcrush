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

namespace {

// A Workday tenant is "ngc.wd1/Northrop_Grumman_External_Site". The site part
// names the company, padded with words like "External Site". Those are taken
// off: "Northrop Grumman".
QString companyNameFromWorkdaySite(const QString &workdayTenant)
{
    const QString siteName = workdayTenant.section(QLatin1Char('/'), 1);
    static const QStringList paddingWords = {
        QStringLiteral("external"), QStringLiteral("site"), QStringLiteral("careers"),
        QStringLiteral("career"), QStringLiteral("jobs"), QStringLiteral("job"),
        QStringLiteral("en"), QStringLiteral("us"),
    };
    QStringList keptWords;
    for (const QString &word : companyNameFromTenant(siteName).split(QLatin1Char(' '),
                                                                     Qt::SkipEmptyParts)) {
        if (!paddingWords.contains(word.toLower())) {
            keptWords.append(word);
        }
    }
    return keptWords.isEmpty() ? companyNameFromTenant(siteName) : keptWords.join(QLatin1Char(' '));
}

} // namespace

QString FollowedEmployerRoster::followEmployerFromLink(const QString &pastedLink,
                                                      const QString &displayName)
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
            "Job Crush can watch companies on Greenhouse, Lever, Ashby, SmartRecruiters "
            "and Workday. That link isn't one of them. Open a job on the company's "
            "careers page. If the address says greenhouse, lever, ashby, "
            "smartrecruiters or myworkdayjobs anywhere in it, that link works.");
    }

    const QString boardDisplayName = AtsBoardName::displayNameFor(boardIdentity.boardName);
    if (boardDisplayName != QStringLiteral("Greenhouse")
            && boardDisplayName != QStringLiteral("Lever")
            && boardDisplayName != QStringLiteral("Ashby")
            && boardDisplayName != QStringLiteral("SmartRecruiters")
            && boardDisplayName != QStringLiteral("Workday")) {
        return QStringLiteral(
            "That company is on %1, and Job Crush can't read %1 yet. It reads "
            "Greenhouse, Lever, Ashby, SmartRecruiters and Workday. It's on the list.")
            .arg(boardDisplayName);
    }

    FollowedEmployer employer;
    employer.boardName = boardIdentity.boardName;
    employer.tenant = boardIdentity.tenant;
    employer.displayName = displayName.trimmed().isEmpty()
        ? (boardIdentity.boardName == AtsBoardName::Workday
               ? companyNameFromWorkdaySite(boardIdentity.tenant)
               : companyNameFromTenant(boardIdentity.tenant))
        : displayName.trimmed();

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

QString FollowedEmployerRoster::followEveryReadableCompanyOn(const CompanyList &companyList)
{
    int companiesAdded = 0;
    int companiesAlreadyWatched = 0;
    int companiesNotReadableYet = 0;

    for (const CompanyListEntry &entry : companyList.companies) {
        if (entry.followLink.isEmpty()) {
            ++companiesNotReadableYet;
            continue;
        }
        const int countBefore = watchedEmployers.count();
        followEmployerFromLink(entry.followLink, entry.companyName);
        if (watchedEmployers.count() > countBefore) {
            ++companiesAdded;
        } else {
            ++companiesAlreadyWatched;
        }
    }

    QStringList sentences;
    sentences.append(companiesAdded == 1
                         ? QStringLiteral("Now watching 1 more company from the list.")
                         : QStringLiteral("Now watching %1 more companies from the list.")
                               .arg(companiesAdded));
    if (companiesAlreadyWatched > 0) {
        sentences.append(QStringLiteral("%1 were already on your list.")
                             .arg(companiesAlreadyWatched));
    }
    if (companiesNotReadableYet > 0) {
        sentences.append(QStringLiteral(
            "%1 others on the list have no feed recorded yet. For those, paste a "
            "job's link into Add a job on Discoveries, or paste it above to watch "
            "the company.").arg(companiesNotReadableYet));
    }
    sentences.append(QStringLiteral("Their jobs turn up the next time you scout."));
    return sentences.join(QLatin1Char(' '));
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
