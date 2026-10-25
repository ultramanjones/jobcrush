#include "AppPreferences.h"

#include <QSettings>

namespace {
// Where each preference is kept in QSettings. These keys are saved on the
// user's machine, so renaming one loses that setting.
const QString boardThemeSettingsKey = QStringLiteral("appearance/boardThemeName");
const QString downloadFormatSettingsKey = QStringLiteral("packets/defaultDownloadFormat");
const QString exportFolderSettingsKey = QStringLiteral("packets/exportFolderPath");
const QString hintsDismissedSettingsKey = QStringLiteral("moonlight/hintsDismissed");
const QString tipsTurnedOnSettingsKey = QStringLiteral("moonlight/tipsTurnedOn");

// What a preference is when nothing has been saved yet.
const QString fallbackBoardThemeName = QStringLiteral("classic");
const QString fallbackDownloadFormat = QStringLiteral("docx");
} // namespace

AppPreferences::AppPreferences(QObject *parent)
    : QObject(parent)
{
}

void AppPreferences::loadFromSettings()
{
    QSettings settings;
    selectedBoardThemeName =
        settings.value(boardThemeSettingsKey, fallbackBoardThemeName).toString();
    selectedDownloadFormat =
        settings.value(downloadFormatSettingsKey, fallbackDownloadFormat).toString();
    selectedExportFolderPath =
        settings.value(exportFolderSettingsKey, QString()).toString();
    // Hidden tips come back each time Job Crush opens (Ultra, 2026-10-16):
    // someone who hid them once may still want them later. Clear the old
    // saved value so a strip hidden by an older build comes back too.
    settings.remove(hintsDismissedSettingsKey);
    tipsTurnedOn = settings.value(tipsTurnedOnSettingsKey, true).toBool();

    // Tell anything already bound that the values may have changed.
    emit boardThemeNameChanged();
    emit defaultDownloadFormatChanged();
    emit exportFolderPathChanged();
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

QString AppPreferences::defaultDownloadFormat() const
{
    return selectedDownloadFormat;
}

void AppPreferences::setDefaultDownloadFormat(const QString &downloadFormat)
{
    if (selectedDownloadFormat == downloadFormat) {
        return;
    }
    selectedDownloadFormat = downloadFormat;

    QSettings settings;
    settings.setValue(downloadFormatSettingsKey, selectedDownloadFormat);

    emit defaultDownloadFormatChanged();
}

QString AppPreferences::exportFolderPath() const
{
    return selectedExportFolderPath;
}

void AppPreferences::setExportFolderPath(const QString &folderPath)
{
    if (selectedExportFolderPath == folderPath) {
        return;
    }
    selectedExportFolderPath = folderPath;

    QSettings settings;
    settings.setValue(exportFolderSettingsKey, selectedExportFolderPath);

    emit exportFolderPathChanged();
}

bool AppPreferences::moonlightHintsAreDismissed() const
{
    return hintsDismissed;
}

void AppPreferences::setMoonlightHintsAreDismissed(bool dismissed)
{
    if (hintsDismissed == dismissed) {
        return;
    }
    hintsDismissed = dismissed; // for this run only, never saved
    emit moonlightHintsDismissedChanged();
}

bool AppPreferences::moonlightTipsAreTurnedOn() const
{
    return tipsTurnedOn;
}

void AppPreferences::setMoonlightTipsAreTurnedOn(bool turnedOn)
{
    if (tipsTurnedOn == turnedOn) {
        return;
    }
    tipsTurnedOn = turnedOn;
    QSettings settings;
    settings.setValue(tipsTurnedOnSettingsKey, turnedOn);
    emit moonlightTipsTurnedOnChanged();
}
