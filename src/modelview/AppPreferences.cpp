#include "AppPreferences.h"

#include <QSettings>

namespace {
const QString boardThemeSettingsKey = QStringLiteral("appearance/boardThemeName");
const QString downloadFormatSettingsKey = QStringLiteral("packets/defaultDownloadFormat");
const QString exportFolderSettingsKey = QStringLiteral("packets/exportFolderPath");
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
    selectedDownloadFormat =
        settings.value(downloadFormatSettingsKey, QStringLiteral("docx")).toString();
    selectedExportFolderPath =
        settings.value(exportFolderSettingsKey, QString()).toString();

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
