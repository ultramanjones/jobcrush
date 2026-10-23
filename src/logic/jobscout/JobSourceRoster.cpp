#include "JobSourceRoster.h"

#include <QSettings>

#include "JobSourceDescriptor.h"

namespace {
const QString enabledSourcesKey = QStringLiteral("jobScout/enabledSourceStorageNames");

// Marks that the roster has been saved at least once, so an empty list can
// mean "the user unticked everything" instead of being mistaken for a fresh
// install and silently re-ticked. Unticking everything is a legitimate
// choice and Job Crush does not argue with it.
const QString rosterHasBeenSavedKey = QStringLiteral("jobScout/rosterHasBeenSaved");

// Where one site's key and email live. Keyed by storage name, so a new site
// needing a key costs nothing here.
QString accessKeySettingsKeyFor(const QString &sourceStorageName)
{
    return QStringLiteral("jobScout/%1/accessKey").arg(sourceStorageName);
}
QString registeredEmailSettingsKeyFor(const QString &sourceStorageName)
{
    return QStringLiteral("jobScout/%1/registeredEmail").arg(sourceStorageName);
}
} // namespace

JobSourceRoster::JobSourceRoster(QObject *parent)
    : QObject(parent)
{
}

void JobSourceRoster::loadFromSettings()
{
    QSettings settings;

    if (!settings.value(rosterHasBeenSavedKey, false).toBool()) {
        // First run: tick everything that works right out of the box. A site
        // needing a free key stays unticked — signing up for something is the
        // user's decision to make, not a surprise on first launch.
        storedEnabledSourceStorageNames.clear();
        for (const JobSourceDescriptor &descriptor : jobSourceCatalog()) {
            if (descriptor.clientIsBuilt && !descriptor.requiresAccessKey) {
                storedEnabledSourceStorageNames.append(descriptor.storageName);
            }
        }
        emit enabledSourcesChanged();
        return;
    }

    storedEnabledSourceStorageNames = settings.value(enabledSourcesKey).toStringList();
    emit enabledSourcesChanged();
}

void JobSourceRoster::persistToSettings() const
{
    QSettings settings;
    settings.setValue(enabledSourcesKey, storedEnabledSourceStorageNames);
    settings.setValue(rosterHasBeenSavedKey, true);
}

bool JobSourceRoster::sourceIsEnabled(const QString &sourceStorageName) const
{
    return storedEnabledSourceStorageNames.contains(sourceStorageName);
}

void JobSourceRoster::setSourceEnabled(const QString &sourceStorageName, bool shouldBeEnabled)
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    if (!descriptorFound || !descriptor.clientIsBuilt) {
        return; // nothing to enable — the UI never offers this box
    }
    if (shouldBeEnabled && !sourceHasWhatItNeeds(sourceStorageName)) {
        return; // no key yet — the UI says so beside the box
    }

    const bool isCurrentlyEnabled = sourceIsEnabled(sourceStorageName);
    if (isCurrentlyEnabled == shouldBeEnabled) {
        return;
    }

    if (shouldBeEnabled) {
        storedEnabledSourceStorageNames.append(sourceStorageName);
    } else {
        storedEnabledSourceStorageNames.removeAll(sourceStorageName);
    }

    persistToSettings();
    emit enabledSourcesChanged();
}

QStringList JobSourceRoster::enabledSourceStorageNames() const
{
    // Catalog order, not the order they happened to get ticked — the tabs on
    // the Discoveries page should not shuffle around between visits.
    QStringList orderedEnabledSources;
    for (const JobSourceDescriptor &descriptor : jobSourceCatalog()) {
        if (storedEnabledSourceStorageNames.contains(descriptor.storageName)
                && sourceHasWhatItNeeds(descriptor.storageName)) {
            orderedEnabledSources.append(descriptor.storageName);
        }
    }
    return orderedEnabledSources;
}

QString JobSourceRoster::accessKeyFor(const QString &sourceStorageName) const
{
    QSettings settings;
    return settings.value(accessKeySettingsKeyFor(sourceStorageName)).toString();
}

void JobSourceRoster::setAccessKeyFor(const QString &sourceStorageName,
                                      const QString &accessKey)
{
    QSettings settings;
    const QString trimmedKey = accessKey.trimmed();
    if (trimmedKey.isEmpty()) {
        settings.remove(accessKeySettingsKeyFor(sourceStorageName));
    } else {
        settings.setValue(accessKeySettingsKeyFor(sourceStorageName), trimmedKey);
    }
    settings.sync();

    // No un-ticking here, on purpose.
    //
    // A site with no key drops out of enabledSourceStorageNames on its own,
    // so it is not swept and gets no tab — the same outcome. Rewriting the
    // ticked list instead would fire enabledSourcesChanged, which rebuilds
    // every row in Settings, which destroys the very box the user is about to
    // type their email into. The tick comes back the moment the key does.
    emit accessKeysChanged();
}

QString JobSourceRoster::registeredEmailFor(const QString &sourceStorageName) const
{
    QSettings settings;
    return settings.value(registeredEmailSettingsKeyFor(sourceStorageName)).toString();
}

void JobSourceRoster::setRegisteredEmailFor(const QString &sourceStorageName,
                                            const QString &registeredEmail)
{
    QSettings settings;
    const QString trimmedEmail = registeredEmail.trimmed();
    if (trimmedEmail.isEmpty()) {
        settings.remove(registeredEmailSettingsKeyFor(sourceStorageName));
    } else {
        settings.setValue(registeredEmailSettingsKeyFor(sourceStorageName), trimmedEmail);
    }
    settings.sync();

    // Same as above: no un-ticking, so the row the user is editing survives.
    emit accessKeysChanged();
}

bool JobSourceRoster::sourceHasWhatItNeeds(const QString &sourceStorageName) const
{
    bool descriptorFound = false;
    const JobSourceDescriptor descriptor =
        jobSourceDescriptorFor(sourceStorageName, descriptorFound);
    if (!descriptorFound || !descriptor.clientIsBuilt) {
        return false;
    }
    if (descriptor.requiresAccessKey && accessKeyFor(sourceStorageName).isEmpty()) {
        return false;
    }
    if (descriptor.requiresRegisteredEmail
            && registeredEmailFor(sourceStorageName).isEmpty()) {
        return false;
    }
    return true;
}
