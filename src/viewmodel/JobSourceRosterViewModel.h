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
    //   requiresAccessKey, clientIsBuilt, isEnabled }
    Q_PROPERTY(QVariantList allJobSources READ allJobSources NOTIFY enabledSourcesChanged)

    // Just the ticked ones, for the Discoveries tabs, in catalog order:
    // { storageName, displayName }
    Q_PROPERTY(QVariantList enabledJobSourceTabs READ enabledJobSourceTabs
                   NOTIFY enabledSourcesChanged)

public:
    explicit JobSourceRosterViewModel(JobSourceRoster &sourceRoster,
                                      QObject *parent = nullptr);

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

signals:
    void enabledSourcesChanged();

private:
    JobSourceRoster &roster;
};
