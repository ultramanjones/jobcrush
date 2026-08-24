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

    // Whether the user has ticked this site. Says nothing about whether it
    // can actually run — see enabledSourceStorageNames for that.
    bool sourceIsEnabled(const QString &sourceStorageName) const;

    // Ticking a site Job Crush cannot talk to yet is refused — the UI never
    // offers that box, so this is a guard rather than a path.
    void setSourceEnabled(const QString &sourceStorageName, bool shouldBeEnabled);

    // The ticked sites that can actually run, in catalog order — the sweep
    // runs these and the Discoveries page draws a tab for each.
    //
    // A site whose key has been cleared drops out here rather than being
    // un-ticked in storage. Same effect for the user, without rewriting the
    // ticked list while they are typing in the box next to it.
    QStringList enabledSourceStorageNames() const;

    // --- The free keys some sites need ---
    //
    // A key is per site and there is only ever one of it, so this is a pair of
    // boxes rather than a roster. Kept behind this class for the same reason
    // AiCredentialRoster keeps its keys behind itself: the day these move out
    // of QSettings and into the operating system's own store, nothing above
    // this file changes.
    //
    // Storage decision (interim, 2026-08-21, same as AIBrain's): QSettings in
    // the application's native settings location. Revisit before release.

    QString accessKeyFor(const QString &sourceStorageName) const;
    void setAccessKeyFor(const QString &sourceStorageName, const QString &accessKey);

    // The email address the key was registered under. USAJOBS sends this on
    // every request and refuses the ones without it.
    QString registeredEmailFor(const QString &sourceStorageName) const;
    void setRegisteredEmailFor(const QString &sourceStorageName,
                               const QString &registeredEmail);

    // True when this site has everything it needs to be asked a question:
    // its client is written, and any key it wants has been filled in.
    bool sourceHasWhatItNeeds(const QString &sourceStorageName) const;

signals:
    void enabledSourcesChanged();

    // A key or an email was filled in or cleared. Settings listens so the
    // checkbox beside it can wake up the moment the key lands.
    void accessKeysChanged();

private:
    void persistToSettings() const;

    QStringList storedEnabledSourceStorageNames;
};
