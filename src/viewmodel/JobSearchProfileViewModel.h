#pragma once

#include <QObject>
#include <QString>

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

    Q_PROPERTY(QString preferredLocationText READ preferredLocationText
                   WRITE setPreferredLocationText NOTIFY searchProfileChanged)

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

    QString preferredLocationText() const;
    void setPreferredLocationText(const QString &locationText);

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
