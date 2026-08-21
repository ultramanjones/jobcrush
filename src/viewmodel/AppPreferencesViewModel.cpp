#include "AppPreferencesViewModel.h"

#include <QDesktopServices>
#include <QUrl>

#include "../modelview/AppPreferences.h"
#include "../modelview/aibrain/AiBrainSoul.h"

AppPreferencesViewModel::AppPreferencesViewModel(AppPreferences &appPreferences,
                                                 AiBrainSoul &aiBrainSoul,
                                                 QObject *parent)
    : QObject(parent)
    , preferences(appPreferences)
    , soul(aiBrainSoul)
{
    connect(&preferences, &AppPreferences::boardThemeNameChanged,
            this, &AppPreferencesViewModel::boardThemeNameChanged);
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
