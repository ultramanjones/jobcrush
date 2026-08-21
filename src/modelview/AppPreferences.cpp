#include "AppPreferences.h"

#include <QSettings>

namespace {
const QString boardThemeSettingsKey = QStringLiteral("appearance/boardThemeName");
} // namespace

AppPreferences::AppPreferences(QObject *parent)
    : QObject(parent)
{
}

void AppPreferences::loadFromSettings()
{
    QSettings settings;
    selectedBoardThemeName =
        settings.value(boardThemeSettingsKey, QStringLiteral("classic")).toString();
    emit boardThemeNameChanged();
}

QString AppPreferences::boardThemeName() const
{
    return selectedBoardThemeName;
}

void AppPreferences::setBoardThemeName(const QString &themeName)
{
    if (selectedBoardThemeName == themeName) {
        return;
    }
    selectedBoardThemeName = themeName;

    QSettings settings;
    settings.setValue(boardThemeSettingsKey, selectedBoardThemeName);

    emit boardThemeNameChanged();
}
