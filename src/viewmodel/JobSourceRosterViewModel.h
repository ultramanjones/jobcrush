#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class JobSourceRoster;

// JobSourceRosterViewModel
//
// Serves the job sites to two places at once: the checkboxes in Settings, and
// the tabs on the Discoveries page. One viewmodel for both, because they are
// two views of the same fact — a ticked site gets swept AND gets a tab, which
// is exactly what the user asked for when they said one box should do both.
class JobSourceRosterViewModel : public QObject {
    Q_OBJECT

    // Every site Job Crush knows about, for the Settings list. Each entry:
    // { storageName, displayName, coverageBlurb, officialSiteUrl,
    //   requiresAccessKey, clientIsBuilt, isEnabled, requiresRegisteredEmail,
    //   accessKeySignupUrl, accessKeyHelpBlurb }
    //
    // The keys themselves are deliberately NOT in here. This list is a model,
    // and changing it rebuilds every row — which would tear down the very box
    // the user is typing a key into. Keys are read one at a time through the
    // methods below, with accessKeyRevision as the nudge to re-read.
    Q_PROPERTY(QVariantList allJobSources READ allJobSources NOTIFY enabledSourcesChanged)

    // Just the ticked ones, for the Discoveries tabs, in catalog order:
    // { storageName, displayName }
    Q_PROPERTY(QVariantList enabledJobSourceTabs READ enabledJobSourceTabs
                   NOTIFY enabledSourcesChanged)

    // Bumped whenever a key or an email changes, so the bindings that depend
    // on one re-evaluate WITHOUT the row list being rebuilt underneath the
    // user. Same technique as the credential roster's revision counter.
    Q_PROPERTY(int accessKeyRevision READ accessKeyRevision NOTIFY accessKeysChanged)

public:
    explicit JobSourceRosterViewModel(JobSourceRoster &sourceRoster,
                                      QObject *parent = nullptr);

    int accessKeyRevision() const;

    QVariantList allJobSources() const;
    QVariantList enabledJobSourceTabs() const;

    // The checkbox. Ticking a site Job Crush cannot talk to yet is refused
    // below — the UI never offers that box in the first place.
    Q_INVOKABLE void setSourceEnabled(const QString &sourceStorageName, bool shouldBeEnabled);

    // Plain speech for the line beside an unavailable checkbox: what this
    // site is waiting on. A fact about the situation, never about the user.
    Q_INVOKABLE QString reasonSourceCannotBeUsed(const QString &sourceStorageName) const;

    // Opens the site's own front door, so the user can see who they are
    // about to pull listings from.
    Q_INVOKABLE void openSourceWebsite(const QString &sourceStorageName) const;

    // --- The free keys some sites need ---

    // Saves the key. Clearing it un-ticks the site, because a ticked site
    // with no key can only fail, once per sweep, forever.
    Q_INVOKABLE void setAccessKey(const QString &sourceStorageName,
                                  const QString &accessKey);

    // The email the key was registered under. USAJOBS sends it on every
    // request and refuses the ones without it.
    Q_INVOKABLE void setRegisteredEmail(const QString &sourceStorageName,
                                        const QString &registeredEmail);

    // Opens the page where the free key is handed out.
    Q_INVOKABLE void openAccessKeySignup(const QString &sourceStorageName) const;

    // True when this site is ready to be asked a question: client written and
    // any key it wants filled in.
    Q_INVOKABLE bool sourceHasWhatItNeeds(const QString &sourceStorageName) const;

    Q_INVOKABLE QString accessKeyFor(const QString &sourceStorageName) const;
    Q_INVOKABLE QString registeredEmailFor(const QString &sourceStorageName) const;

signals:
    void enabledSourcesChanged();
    void accessKeysChanged();

private:
    JobSourceRoster &roster;
    int accessKeyChangeCounter = 0;
};
