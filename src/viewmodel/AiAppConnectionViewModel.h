#pragma once

#include <QObject>
#include <QString>

class AiAppConnection;

// AiAppConnectionViewModel
//
// Serves the "AI apps" part of Settings: whether AI apps may work Job Crush
// directly, and the text to paste into Claude desktop to set it up.
class AiAppConnectionViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool connectionIsOpen READ connectionIsOpen WRITE setConnectionIsOpen
                   NOTIFY connectionChanged)
    Q_PROPERTY(QString problemText READ problemText NOTIFY connectionChanged)
    Q_PROPERTY(QString claudeDesktopSettingsText READ claudeDesktopSettingsText CONSTANT)
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY lastActionChanged)

public:
    explicit AiAppConnectionViewModel(AiAppConnection &aiAppConnection,
                                      QObject *parent = nullptr);

    bool connectionIsOpen() const;
    void setConnectionIsOpen(bool shouldBeOpen);
    QString problemText() const;
    QString claudeDesktopSettingsText() const;
    QString lastActionText() const;

    // Puts the Claude desktop settings text on the clipboard.
    Q_INVOKABLE void copyClaudeDesktopSettings();

signals:
    void connectionChanged();
    void lastActionChanged();

private:
    AiAppConnection &connection;
    QString lastActionSentence;
};
