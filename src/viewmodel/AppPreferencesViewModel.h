#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class AiBrainSoul;
class AppPreferences;

// AppPreferencesViewModel
//
// Serves app-wide preference values to the view and forwards changes down.
// Also hands the Settings page the soul folder location (with an action to
// open it), since "soul-file access" is a Settings surface per the plan.
class AppPreferencesViewModel : public QObject {
    Q_OBJECT

    // "classic" | "grayscale" — the QML theme singleton binds to this.
    Q_PROPERTY(QString boardThemeName READ boardThemeName WRITE setBoardThemeName
                   NOTIFY boardThemeNameChanged)

    // The hints strip on the Moonlight chat page: the hints to rotate
    // through, and whether the user closed the strip.
    Q_PROPERTY(QStringList moonlightHints READ moonlightHints CONSTANT)
    Q_PROPERTY(bool moonlightHintsAreDismissed READ moonlightHintsAreDismissed
                   WRITE setMoonlightHintsAreDismissed NOTIFY moonlightHintsDismissedChanged)
    // The Settings checkbox. Off hides the tips for good.
    Q_PROPERTY(bool moonlightTipsAreTurnedOn READ moonlightTipsAreTurnedOn
                   WRITE setMoonlightTipsAreTurnedOn NOTIFY moonlightTipsTurnedOnChanged)

    // Where the soul text files live, shown in Settings.
    Q_PROPERTY(QString soulFolderPath READ soulFolderPath CONSTANT)

public:
    AppPreferencesViewModel(AppPreferences &appPreferences,
                            AiBrainSoul &aiBrainSoul,
                            QObject *parent = nullptr);

    QString boardThemeName() const;
    void setBoardThemeName(const QString &themeName);

    QString soulFolderPath() const;

    QStringList moonlightHints() const;
    bool moonlightHintsAreDismissed() const;
    void setMoonlightHintsAreDismissed(bool dismissed);
    bool moonlightTipsAreTurnedOn() const;
    void setMoonlightTipsAreTurnedOn(bool turnedOn);

    // Opens the soul folder in the system file manager, and reloads the soul
    // files on the way (cheap, and it means edits are picked up by the very
    // next AIBrain request without restarting).
    Q_INVOKABLE void openSoulFolder();

signals:
    void boardThemeNameChanged();
    void moonlightHintsDismissedChanged();
    void moonlightTipsTurnedOnChanged();

private:
    AppPreferences &preferences;
    AiBrainSoul &soul;
};
