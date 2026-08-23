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
        return QString(); // nothing standing in the way
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
