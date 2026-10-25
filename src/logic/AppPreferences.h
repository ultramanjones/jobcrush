#pragma once

#include <QObject>
#include <QString>

// AppPreferences
//
// The user's app-wide preferences, saved with QSettings. A Logic layer class
// that wraps the storage, so nothing above it knows where preferences live.
// Settings apply at once: every setter saves right away and sends its
// changed signal. There is no Apply or OK button anywhere.
//
// It holds the board color theme, the packet export format and folder, and
// the Moonlight tips settings.
class AppPreferences : public QObject {
    Q_OBJECT
public:
    explicit AppPreferences(QObject *parent = nullptr);

    // Loads persisted values. Called once from the composition root.
    void loadFromSettings();

    // Board color theme: "classic" (bright 1980s neon colors, the default) or
    // "grayscale" (the same dark base, told apart by shade, for eyes that
    // find bright color hard).
    QString boardThemeName() const;
    void setBoardThemeName(const QString &themeName);

    // The format used when exporting a packet: "docx" or "pdf". See
    // ExportFormat.h. It is a saved default so the user is not asked on every
    // export. The Staging page still shows both choices.
    QString defaultDownloadFormat() const;
    void setDefaultDownloadFormat(const QString &downloadFormat);

    // Where exported packets are saved. Empty means use the default:
    // Documents/Job Crush Packets.
    QString exportFolderPath() const;
    void setExportFolderPath(const QString &folderPath);

    // Whether the user hid the tips strip on the Moonlight chat page. Hidden
    // until Job Crush is closed. The tips come back the next time it opens.
    bool moonlightHintsAreDismissed() const;
    void setMoonlightHintsAreDismissed(bool dismissed);

    // The Settings checkbox "Show tips on the Moonlight page". Off means the
    // tips never show, and that is saved.
    bool moonlightTipsAreTurnedOn() const;
    void setMoonlightTipsAreTurnedOn(bool turnedOn);

signals:
    void boardThemeNameChanged();
    void moonlightHintsDismissedChanged();
    void moonlightTipsTurnedOnChanged();
    void defaultDownloadFormatChanged();
    void exportFolderPathChanged();

private:
    // Defaults until loadFromSettings runs. They match the defaults in the .cpp.
    QString selectedBoardThemeName = QStringLiteral("classic");
    QString selectedDownloadFormat = QStringLiteral("docx");
    QString selectedExportFolderPath;
    bool hintsDismissed = false;
    bool tipsTurnedOn = true;
};
