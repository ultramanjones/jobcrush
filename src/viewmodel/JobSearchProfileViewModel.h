#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class JobSearchProfile;

// JobSearchProfileViewModel
//
// Serves the "what am I looking for?" fields to Settings. The profile keeps
// lists; a person types a line. This class is the translation between those
// two shapes and nothing more.
//
// Every setter saves the moment it is called. There is no Apply or OK
// button anywhere.
class JobSearchProfileViewModel : public QObject {
    Q_OBJECT

    // "Qt Developer, C++ Engineer" — one line in, a list out.
    Q_PROPERTY(QString targetJobTitlesText READ targetJobTitlesText
                   WRITE setTargetJobTitlesText NOTIFY searchProfileChanged)

    // "Qt, QML, C++, MVVM"
    Q_PROPERTY(QString skillKeywordsText READ skillKeywordsText
                   WRITE setSkillKeywordsText NOTIFY searchProfileChanged)

    // A list, not a line. Chips let "Pittsburgh, PA" be one entry, comma and
    // all. A comma-separated box would split it in two.
    Q_PROPERTY(QStringList preferredWorkLocations READ preferredWorkLocations
                   NOTIFY searchProfileChanged)

    Q_PROPERTY(bool remoteRolesOnly READ remoteRolesOnly
                   WRITE setRemoteRolesOnly NOTIFY searchProfileChanged)

    // Zero means "not a factor" — shown as an empty box, never as "0".
    Q_PROPERTY(QString minimumSalaryText READ minimumSalaryText
                   WRITE setMinimumSalaryText NOTIFY searchProfileChanged)

    Q_PROPERTY(bool hasEnoughToRankBy READ hasEnoughToRankBy NOTIFY searchProfileChanged)

public:
    explicit JobSearchProfileViewModel(JobSearchProfile &searchProfile,
                                       QObject *parent = nullptr);

    QString targetJobTitlesText() const;
    void setTargetJobTitlesText(const QString &jobTitlesText);

    QString skillKeywordsText() const;
    void setSkillKeywordsText(const QString &keywordsText);

    QStringList preferredWorkLocations() const;

    // The chip actions. Adding a place that is not in the suggestion list is
    // fine. Suggestions only help with typing. They never limit what the user
    // may enter, because the user's town may not be on the list.
    Q_INVOKABLE void addWorkLocation(const QString &workLocation);
    Q_INVOKABLE void removeWorkLocationAt(int locationIndex);

    // What to offer while the user types. A built-in list, so it answers
    // instantly, works offline, and sends nothing anywhere.
    Q_INVOKABLE QStringList workLocationSuggestions(const QString &typedPrefix) const;

    bool remoteRolesOnly() const;
    void setRemoteRolesOnly(bool remoteOnly);

    QString minimumSalaryText() const;
    void setMinimumSalaryText(const QString &salaryText);

    bool hasEnoughToRankBy() const;

    // Adds the C++ and Qt preset's job titles and skills to the profile.
    // Nothing already typed is removed. Returns the sentence to show.
    Q_INVOKABLE QString addTheCppQtPreset();

signals:
    void searchProfileChanged();

private:
    JobSearchProfile &profile;
};
