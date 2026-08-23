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
        if (storedEnabledSourceStorageNames.contains(descriptor.storageName)) {
            orderedEnabledSources.append(descriptor.storageName);
        }
    }
    return orderedEnabledSources;
}
