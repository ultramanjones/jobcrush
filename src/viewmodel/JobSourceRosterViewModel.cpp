#include "JobSourceRosterViewModel.h"

#include <QDesktopServices>
#include <QUrl>
#include <QVariantMap>

#include "../modelview/jobscout/JobSourceDescriptor.h"
#include "../modelview/jobscout/JobSourceRoster.h"

JobSourceRosterViewModel::JobSourceRosterViewModel(JobSourceRoster &sourceRoster,
                                                   QObject *parent)
    : QObject(parent)
    , roster(sourceRoster)
{
    connect(&roster, &JobSourceRoster::enabledSourcesChanged,
            this, &JobSourceRosterViewModel::enabledSourcesChanged);

    // A key changing must NOT rebuild the row list. The row the user is
    // typing into would be destroyed and rebuilt under their hands, and
    // whatever they had half-typed in the next box would go with it. So this
    // only bumps a counter, and the handful of bindings that care re-read.
    connect(&roster, &JobSourceRoster::accessKeysChanged, this, [this]() {
        ++accessKeyChangeCounter;
        emit accessKeysChanged();
    });
}

QVariantList JobSourceRosterViewModel::allJobSources() const
{
    QVariantList jobSourceRows;
    for (const JobSourceDescriptor &descriptor : jobSourceCatalog()) {
        QVariantMap jobSourceRow;
        jobSourceRow.insert(QStringLiteral("storageName"), descriptor.storageName);
        jobSourceRow.insert(QStringLiteral("displayName"), descriptor.displayName);
        jobSourceRow.insert(QStringLiteral("coverageBlurb"), descriptor.coverageBlurb);
        jobSourceRow.insert(QStringLiteral("officialSiteUrl"), descriptor.officialSiteUrl);
        jobSourceRow.insert(QStringLiteral("requiresAccessKey"), descriptor.requiresAccessKey);
        jobSourceRow.insert(QStringLiteral("clientIsBuilt"), descriptor.clientIsBuilt);
        jobSourceRow.insert(QStringLiteral("isEnabled"),
                            roster.sourceIsEnabled(descriptor.storageName));
        jobSourceRow.insert(QStringLiteral("requiresRegisteredEmail"),
                            descriptor.requiresRegisteredEmail);
        jobSourceRow.insert(QStringLiteral("accessKeySignupUrl"),
                            descriptor.accessKeySignupUrl);
        jobSourceRow.insert(QStringLiteral("accessKeyHelpBlurb"),
                            descriptor.accessKeyHelpBlurb);
        jobSourceRows.append(jobSourceRow);
    }
    return jobSourceRows;
}

QVariantList JobSourceRosterViewModel::enabledJobSourceTabs() const
{
    QVariantList jobSourceTabs;
    for (const QString &sourceStorageName : roster.enabledSourceStorageNames()) {
        bool descriptorFound = false;
        const JobSourceDescriptor descriptor =
            jobSourceDescriptorFor(sourceStorageName, descriptorFound);
        if (!descriptorFound) {
            continue;
        }
        QVariantMap jobSourceTab;
        jobSourceTab.insert(QStringLiteral("storageName"), descriptor.storageName);
        jobSourceTab.insert(QStringLiteral("displayName"), descriptor.displayName);
        jobSourceTabs.append(jobSourceTab);
    }
    return jobSourceTabs;
}

void JobSourceRosterViewModel::setSourceEnabled(const QString &sourceStorageName,
                                                bool shouldBeEnabled)
{
    roster.setSourceEnabled(sourceStorageName, shouldBeEnabled);
}

QString JobSourceRosterViewModel::reasonSourceCannotBeUsed(
    const QString &sourceStorageName) const
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    if (!descriptorFound) {
        return QString();
    }
    if (descriptor.clientIsBuilt) {
        if (roster.sourceHasWhatItNeeds(sourceStorageName)) {
            return QString(); // nothing standing in the way
        }
        if (descriptor.requiresRegisteredEmail
                && !roster.accessKeyFor(sourceStorageName).isEmpty()
                && roster.registeredEmailFor(sourceStorageName).isEmpty()) {
            return QStringLiteral("%1 also needs the email address you registered the "
                                  "key with. Put it in below.").arg(descriptor.displayName);
        }
        return QStringLiteral("Put %1's free key in below and this box wakes up.")
            .arg(descriptor.displayName);
    }

    if (descriptor.requiresAccessKey) {
        return QStringLiteral("Coming soon — %1 needs a free sign-up, and Job Crush "
                              "hasn't learned to use it yet.").arg(descriptor.displayName);
    }
    return QStringLiteral("Coming soon — Job Crush hasn't learned to read %1 yet.")
        .arg(descriptor.displayName);
}

void JobSourceRosterViewModel::openSourceWebsite(const QString &sourceStorageName) const
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    if (!descriptorFound || descriptor.officialSiteUrl.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl(descriptor.officialSiteUrl));
}

void JobSourceRosterViewModel::setAccessKey(const QString &sourceStorageName,
                                            const QString &accessKey)
{
    roster.setAccessKeyFor(sourceStorageName, accessKey);
}

void JobSourceRosterViewModel::setRegisteredEmail(const QString &sourceStorageName,
                                                  const QString &registeredEmail)
{
    roster.setRegisteredEmailFor(sourceStorageName, registeredEmail);
}

void JobSourceRosterViewModel::openAccessKeySignup(const QString &sourceStorageName) const
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    if (!descriptorFound || descriptor.accessKeySignupUrl.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl(descriptor.accessKeySignupUrl));
}

int JobSourceRosterViewModel::accessKeyRevision() const
{
    return accessKeyChangeCounter;
}

bool JobSourceRosterViewModel::sourceHasWhatItNeeds(const QString &sourceStorageName) const
{
    return roster.sourceHasWhatItNeeds(sourceStorageName);
}

QString JobSourceRosterViewModel::accessKeyFor(const QString &sourceStorageName) const
{
    return roster.accessKeyFor(sourceStorageName);
}

QString JobSourceRosterViewModel::registeredEmailFor(const QString &sourceStorageName) const
{
    return roster.registeredEmailFor(sourceStorageName);
}
