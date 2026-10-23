#include "AppPreferencesViewModel.h"

#include <QDesktopServices>
#include <QUrl>

#include "../logic/AppPreferences.h"
#include "../logic/aibrain/AiBrainSoul.h"
#include "../logic/brainchat/MoonlightHints.h"

AppPreferencesViewModel::AppPreferencesViewModel(AppPreferences &appPreferences,
                                                 AiBrainSoul &aiBrainSoul,
                                                 QObject *parent)
    : QObject(parent)
    , preferences(appPreferences)
    , soul(aiBrainSoul)
{
    connect(&preferences, &AppPreferences::boardThemeNameChanged,
            this, &AppPreferencesViewModel::boardThemeNameChanged);
    connect(&preferences, &AppPreferences::moonlightHintsDismissedChanged,
            this, &AppPreferencesViewModel::moonlightHintsDismissedChanged);
    connect(&preferences, &AppPreferences::moonlightTipsTurnedOnChanged,
            this, &AppPreferencesViewModel::moonlightTipsTurnedOnChanged);
}

QString AppPreferencesViewModel::boardThemeName() const
{
    return preferences.boardThemeName();
}

void AppPreferencesViewModel::setBoardThemeName(const QString &themeName)
{
    preferences.setBoardThemeName(themeName);
}

QString AppPreferencesViewModel::soulFolderPath() const
{
    return soul.soulFolderPath();
}

void AppPreferencesViewModel::openSoulFolder()
{
    // Reload first so any edits already made apply to the next request,
    // then hand the folder to the OS file manager.
    soul.loadCreatingDefaultsIfMissing();
    QDesktopServices::openUrl(QUrl::fromLocalFile(soul.soulFolderPath()));
}

QStringList AppPreferencesViewModel::moonlightHints() const
{
    return MoonlightHints::everyHint();
}

bool AppPreferencesViewModel::moonlightHintsAreDismissed() const
{
    return preferences.moonlightHintsAreDismissed();
}

void AppPreferencesViewModel::setMoonlightHintsAreDismissed(bool dismissed)
{
    preferences.setMoonlightHintsAreDismissed(dismissed);
}

bool AppPreferencesViewModel::moonlightTipsAreTurnedOn() const
{
    return preferences.moonlightTipsAreTurnedOn();
}

void AppPreferencesViewModel::setMoonlightTipsAreTurnedOn(bool turnedOn)
{
    preferences.setMoonlightTipsAreTurnedOn(turnedOn);
}
