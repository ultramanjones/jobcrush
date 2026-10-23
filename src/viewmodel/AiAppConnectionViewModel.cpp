#include "AiAppConnectionViewModel.h"

#include <QClipboard>
#include <QGuiApplication>

#include "../logic/aiapps/AiAppConnection.h"

AiAppConnectionViewModel::AiAppConnectionViewModel(AiAppConnection &aiAppConnection,
                                                   QObject *parent)
    : QObject(parent)
    , connection(aiAppConnection)
{
    connect(&connection, &AiAppConnection::openChanged,
            this, &AiAppConnectionViewModel::connectionChanged);
}

bool AiAppConnectionViewModel::connectionIsOpen() const
{
    return connection.isOpen();
}

void AiAppConnectionViewModel::setConnectionIsOpen(bool shouldBeOpen)
{
    connection.setOpen(shouldBeOpen);
}

QString AiAppConnectionViewModel::problemText() const
{
    return connection.problemText();
}

QString AiAppConnectionViewModel::claudeDesktopSettingsText() const
{
    return AiAppConnection::claudeDesktopSettingsText();
}

QString AiAppConnectionViewModel::lastActionText() const
{
    return lastActionSentence;
}

void AiAppConnectionViewModel::copyClaudeDesktopSettings()
{
    QGuiApplication::clipboard()->setText(AiAppConnection::claudeDesktopSettingsText());
    lastActionSentence = QStringLiteral("Copied. Paste it into Claude desktop's settings "
                                        "file, save it, and restart Claude desktop.");
    emit lastActionChanged();
}
