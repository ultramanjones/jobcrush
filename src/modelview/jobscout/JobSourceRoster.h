#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// JobSourceRoster
//
// Which job sites the user has ticked. A ModelView resident owning its own
// persistence (QSettings), exactly like AiCredentialRoster — the pattern is
// deliberate: two rosters, one shape, one thing to learn.
//
// The tick does double duty by the user's design: an unticked site is never
// swept AND gets no tab on the Discoveries page. One box, one meaning.
class JobSourceRoster : public QObject {
    Q_OBJECT
public:
    explicit JobSourceRoster(QObject *parent = nullptr);

    // Loads the ticked sites. Called once from the composition root.
    // First run ticks every site whose client is built and needs no key, so
    // a brand-new install finds jobs without being configured first.
    void loadFromSettings();

    bool sourceIsEnabled(const QString &sourceStorageName) const;

    // Ticking a site Job Crush cannot talk to yet is refused — the UI never
    // offers that box, so this is a guard rather than a path.
    void setSourceEnabled(const QString &sourceStorageName, bool shouldBeEnabled);

    // The ticked sites, in catalog order — the sweep runs these and the
    // Discoveries page draws a tab for each.
    QStringList enabledSourceStorageNames() const;

signals:
    void enabledSourcesChanged();

private:
    void persistToSettings() const;

    QStringList storedEnabledSourceStorageNames;
};
