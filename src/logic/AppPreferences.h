#pragma once

#include <QObject>
#include <QString>

// AppPreferences
//
// The user's app-wide preferences, persisted via QSettings — a Logic layer
// class wrapping the storage so nothing above it knows where preferences
// live. Settings are instant-apply by law: every setter persists immediately
// and signals, no Apply/OK ceremony anywhere.
//
// Phase 3 holds the board color theme; the juice dial (Off/Subtle/Full
// Arcade) joins in Phase 7.
class AppPreferences : public QObject {
    Q_OBJECT
public:
    explicit AppPreferences(QObject *parent = nullptr);

    // Loads persisted values. Called once from the composition root.
    void loadFromSettings();

    // Board color theme: "classic" (retro-80s neons, default) or
    // "grayscale" (same dark base, shade-distinguished — for sensitive eyes).
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
    QString selectedBoardThemeName = QStringLiteral("classic");
    QString selectedDownloadFormat = QStringLiteral("docx");
    QString selectedExportFolderPath;
    bool hintsDismissed = false;
    bool tipsTurnedOn = true;
};
