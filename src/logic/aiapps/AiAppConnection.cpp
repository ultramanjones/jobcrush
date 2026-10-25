#include "AiAppConnection.h"

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLocalSocket>
#include <QSettings>

#include "AiAppRequests.h"
#include "AiAppTools.h"

namespace {

const QString doorOpenSettingKey = QStringLiteral("aiApps/connectionIsOpen");

// A line longer than this is not a request; it is a mistake or an attack.
constexpr int longestLineAccepted = 1024 * 1024;

} // namespace

AiAppConnection::AiAppConnection(AiAppRequests &aiAppRequests, QObject *parent)
    : QObject(parent)
    , requests(aiAppRequests)
{
    // Only programs run by this same user may connect.
    localServer.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&localServer, &QLocalServer::newConnection,
            this, &AiAppConnection::acceptNewConnection);
}

void AiAppConnection::loadFromSettings()
{
    QSettings settings;
    doorShouldBeOpen = settings.value(doorOpenSettingKey, true).toBool();
    if (doorShouldBeOpen) {
        openDoor();
    }
}

bool AiAppConnection::isOpen() const
{
    return localServer.isListening();
}

void AiAppConnection::setOpen(bool shouldBeOpen)
{
    doorShouldBeOpen = shouldBeOpen;
    QSettings settings;
    settings.setValue(doorOpenSettingKey, shouldBeOpen);
    if (shouldBeOpen) {
        openDoor();
    } else {
        closeDoor();
    }
    emit openChanged();
}

QString AiAppConnection::problemText() const
{
    return doorProblemText;
}

void AiAppConnection::openDoor()
{
    if (localServer.isListening()) {
        return;
    }
    // On Linux and Mac a Job Crush that crashed leaves its old door behind,
    // and a new one cannot open until it is cleared. On Windows this does
    // nothing.
    QLocalServer::removeServer(AiAppTools::ConnectionName);
    if (localServer.listen(AiAppTools::ConnectionName)) {
        doorProblemText.clear();
    } else {
        qWarning().noquote() << QStringLiteral(
            "AiAppConnection::openDoor: could not listen on \"%1\" — %2")
            .arg(AiAppTools::ConnectionName, localServer.errorString());
        doorProblemText = QStringLiteral(
            "AI apps cannot reach Job Crush: %1. If Job Crush is open twice, close "
            "one of them.").arg(localServer.errorString());
    }
}

void AiAppConnection::closeDoor()
{
    localServer.close();
    doorProblemText.clear();
}

// Takes every waiting connection. Each one is read as data arrives, and
// its buffer and socket are dropped when the bridge hangs up.
void AiAppConnection::acceptNewConnection()
{
    while (QLocalSocket *connection = localServer.nextPendingConnection()) {
        connect(connection, &QLocalSocket::readyRead, this,
                [this, connection]() { readFromConnection(connection); });
        connect(connection, &QLocalSocket::disconnected, this, [this, connection]() {
            unfinishedLines.remove(connection);
            connection->deleteLater();
        });
    }
}

void AiAppConnection::readFromConnection(QLocalSocket *connection)
{
    // Each connection keeps its own buffer of bytes that do not yet make a
    // whole line. The steps:
    //  1. Add whatever bytes arrived to the end of that buffer.
    //  2. While the buffer holds a line break, cut off one complete line.
    //  3. Read the line as one JSON-RPC message and write one answer line
    //     back. A notification gets no answer.
    //  4. Bytes after the last line break wait for the next readyRead.
    //  5. If those waiting bytes grow past longestLineAccepted, the
    //     connection is dropped.
    QByteArray &unfinishedBytes = unfinishedLines[connection];
    unfinishedBytes += connection->readAll();

    int lineBreakIndex = unfinishedBytes.indexOf('\n');
    while (lineBreakIndex >= 0) {
        const QByteArray line = unfinishedBytes.left(lineBreakIndex).trimmed();
        unfinishedBytes.remove(0, lineBreakIndex + 1);
        lineBreakIndex = unfinishedBytes.indexOf('\n');
        if (line.isEmpty()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument message = QJsonDocument::fromJson(line, &parseError);
        QJsonObject answer;
        if (parseError.error != QJsonParseError::NoError || !message.isObject()) {
            answer = AiAppTools::errorMessage(QJsonValue(), AiAppTools::ParseError,
                                              QStringLiteral("Job Crush could not read that "
                                                             "message as JSON."));
        } else {
            answer = answerFor(message.object());
        }
        if (!answer.isEmpty()) {
            connection->write(QJsonDocument(answer).toJson(QJsonDocument::Compact) + '\n');
            connection->flush();
        }
    }

    if (unfinishedBytes.size() > longestLineAccepted) {
        qWarning().noquote() << QStringLiteral(
            "AiAppConnection::readFromConnection: dropped an AI app connection — "
            "%1 bytes arrived with no line break (the limit is %2)")
            .arg(unfinishedBytes.size()).arg(longestLineAccepted);
        connection->disconnectFromServer();
    }
}

QString AiAppConnection::bridgeProgramPath()
{
#ifdef Q_OS_WIN
    const QString programName = QStringLiteral("jobcrush-ai-bridge.exe");
#else
    const QString programName = QStringLiteral("jobcrush-ai-bridge");
#endif
    return QDir::toNativeSeparators(
        QDir(QCoreApplication::applicationDirPath()).filePath(programName));
}

QString AiAppConnection::claudeDesktopSettingsText()
{
    QJsonObject bridgeEntry { { QStringLiteral("command"), bridgeProgramPath() } };

    // An installed Job Crush keeps Qt's files beside itself, and the bridge
    // finds them there. A Job Crush run from a build folder does not, so the
    // bridge is told where Qt is.
    const QString qtProgramsFolder =
        QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::BinariesPath));
    const QString jobCrushFolder =
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    if (!qtProgramsFolder.isEmpty()
            && QDir(qtProgramsFolder) != QDir(jobCrushFolder)
            && QFileInfo::exists(qtProgramsFolder)) {
        bridgeEntry.insert(QStringLiteral("env"),
                           QJsonObject { { QStringLiteral("PATH"), qtProgramsFolder } });
    }

    const QJsonObject settings {
        { QStringLiteral("mcpServers"),
          QJsonObject { { AiAppTools::ServerName, bridgeEntry } } }
    };
    return QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Indented)).trimmed();
}

// Answers the four MCP methods Job Crush knows: initialize, ping,
// tools/list and tools/call. Anything else gets JSON-RPC's "method not
// found" error.
QJsonObject AiAppConnection::answerFor(const QJsonObject &message) const
{
    const QString method = message.value(QStringLiteral("method")).toString();
    const QJsonValue requestId = message.value(QStringLiteral("id"));
    const QJsonObject parameters = message.value(QStringLiteral("params")).toObject();

    // A message with no id is a notification, and gets no answer.
    if (requestId.isUndefined()) {
        return {};
    }

    if (method == QStringLiteral("initialize")) {
        return AiAppTools::resultMessage(
            requestId,
            AiAppTools::initializeResult(
                parameters.value(QStringLiteral("protocolVersion")).toString()));
    }
    if (method == QStringLiteral("ping")) {
        return AiAppTools::resultMessage(requestId, {});
    }
    if (method == QStringLiteral("tools/list")) {
        return AiAppTools::resultMessage(
            requestId, { { QStringLiteral("tools"), AiAppTools::toolDefinitions() } });
    }
    if (method == QStringLiteral("tools/call")) {
        const AiAppAnswer answer = requests.runAction(
            parameters.value(QStringLiteral("name")).toString(),
            parameters.value(QStringLiteral("arguments")).toObject(),
            QDate::currentDate());
        return AiAppTools::resultMessage(requestId,
                                         AiAppTools::toolCallResult(answer.text, answer.isError));
    }
    return AiAppTools::errorMessage(requestId, AiAppTools::MethodNotFound,
                                    QStringLiteral("Job Crush does not know \"%1\".")
                                        .arg(method));
}
