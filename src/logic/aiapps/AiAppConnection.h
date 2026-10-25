#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QString>

class AiAppRequests;
class QLocalSocket;

// AiAppConnection
//
// The door AI apps use to ask Job Crush to do things.
//
// An AI app such as Claude desktop starts the small bridge program in
// tools/ai-app-bridge. The bridge passes the AI's requests to this door, a
// local connection on this computer only. Windows lets only programs run by
// the same user open it. Nothing here is reachable from the network.
//
// Messages are MCP's JSON-RPC, one per line. This class reads each line,
// hands actions to AiAppRequests, and writes the answer back as one line.
class AiAppConnection : public QObject {
    Q_OBJECT
public:
    explicit AiAppConnection(AiAppRequests &aiAppRequests, QObject *parent = nullptr);

    // Reads whether the user has turned the door off. Called once from the
    // composition root.
    void loadFromSettings();

    // Opens or closes the door, and remembers the choice.
    bool isOpen() const;
    void setOpen(bool shouldBeOpen);

    // Why the door could not open, in a sentence. Empty when all is well.
    QString problemText() const;

    // Where the bridge program is: next to Job Crush itself.
    static QString bridgeProgramPath();

    // The lines to paste into Claude desktop's settings file so it starts
    // the bridge. Also names Qt's own folder when Job Crush runs from a
    // build folder, where the bridge cannot find Qt's files on its own.
    static QString claudeDesktopSettingsText();

    // The answer to one JSON-RPC message, or an empty object when the
    // message needs no answer (a notification). Public so it can be tested
    // without a connection.
    QJsonObject answerFor(const QJsonObject &message) const;

signals:
    void openChanged();

private:
    // Starts or stops listening. A failure to start is kept in problemText.
    void openDoor();
    void closeDoor();

    void acceptNewConnection();

    // Reads whole lines from one bridge and writes back one answer per line.
    void readFromConnection(QLocalSocket *connection);

    AiAppRequests &requests;
    QLocalServer localServer;
    // Bytes from each bridge that do not yet make a whole line.
    QHash<QLocalSocket *, QByteArray> unfinishedLines;
    bool doorShouldBeOpen = true;
    QString doorProblemText;
};
