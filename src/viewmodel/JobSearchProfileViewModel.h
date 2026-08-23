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
// Instant-apply by law: every setter persists the moment it happens, no
// Apply/OK ceremony anywhere.
class JobSearchProfileViewModel : public QObject {
    Q_OBJECT

    // "Qt Developer, C++ Engineer" — one line in, a list out.
    Q_PROPERTY(QString targetJobTitlesText READ targetJobTitlesText
                   WRITE setTargetJobTitlesText NOTIFY searchProfileChanged)

    // "Qt, QML, C++, MVVM"
    Q_PROPERTY(QString skillKeywordsText READ skillKeywordsText
                   WRITE setSkillKeywordsText NOTIFY searchProfileChanged)

    // A list, not a line: chips let "Pittsburgh, PA" be one entry, comma and
    // all, which a comma-separated box could never do.
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

    // The chip actions. Adding something not in the suggestion list is fine —
    // suggestions are a convenience, never a gate, and an autocomplete that
    // refuses unfamiliar input is a form telling someone they live in the
    // wrong place.
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

signals:
    void searchProfileChanged();

private:
    JobSearchProfile &profile;
};
