#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// JobSearchProfile
//
// What the user is actually looking for: the target titles, the skills they
// bring, where they will work, and the floor they will accept. A ModelView
// resident, persisted in QSettings alongside the rest of the app's
// preferences — app-wide, no accounts, set it and forget it.
//
// This is the input to TWO things: what JobScout asks each site for, and what
// ProspectScorer ranks the results against.
//
// FUTURE (ProDocs, Phase 5): the skill list is the seam. Once professional
// documents are loaded, the words pulled out of a resume join this list
// automatically, so the same algorithm gets a much richer picture without a
// line of scoring code changing. Typing skills here stays supported — some
// people know exactly what they want to be found for.
class JobSearchProfile : public QObject {
    Q_OBJECT
public:
    explicit JobSearchProfile(QObject *parent = nullptr);

    // Loads persisted values. Called once from the composition root.
    void loadFromSettings();

    // The roles being hunted, most wanted first: "Qt Developer",
    // "C++ Engineer". Drives both the search query and the biggest slice of
    // the prospect score.
    QStringList targetJobTitles() const;
    void setTargetJobTitles(const QStringList &jobTitles);

    // What the user brings: "Qt", "QML", "C++", "MVVM". Today typed by hand;
    // tomorrow topped up from ProDocs.
    QStringList skillKeywords() const;
    void setSkillKeywords(const QStringList &keywords);

    // Free text, matched loosely: "Austin", "Texas", "United States".
    QString preferredLocationText() const;
    void setPreferredLocationText(const QString &locationText);

    // When true, a job that is not remote scores no location points at all.
    bool remoteRolesOnly() const;
    void setRemoteRolesOnly(bool remoteOnly);

    // Yearly figure. Zero means "not a factor" — Job Crush never invents a
    // number the user did not give it.
    int minimumAcceptableSalary() const;
    void setMinimumAcceptableSalary(int yearlySalary);

    // True once there is enough here to rank anything at all. The Top
    // Prospects tab says so plainly rather than showing a meaningless order.
    bool hasEnoughToRankBy() const;

    // Everything the profile knows about what the user brings, lowercased and
    // deduplicated — titles and skills together. One place builds this so the
    // scorer and any future contributor (ProDocs) agree on the shape.
    QStringList matchableTermsLowercased() const;

signals:
    void searchProfileChanged();

private:
    void persistToSettings() const;

    QStringList storedTargetJobTitles;
    QStringList storedSkillKeywords;
    QString storedPreferredLocationText;
    bool storedRemoteRolesOnly = false;
    int storedMinimumAcceptableSalary = 0;
};
