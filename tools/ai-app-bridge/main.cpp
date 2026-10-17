// jobcrush-ai-bridge
//
// The small program an AI app starts to talk to Job Crush.
//
// AI apps such as Claude desktop speak MCP (Model Context Protocol). They
// start a program and talk to it through its standard input and output, one
// JSON message per line. Job Crush is a window app and cannot be started that
// way, and a second copy of Job Crush must never open the database. So the AI
// app starts this bridge instead, and the bridge passes each action on to the
// Job Crush that is already running, through a local connection on this
// computer.
//
// The bridge answers the first questions itself: who it is, and what actions
// Job Crush offers. That way the AI app can start even while Job Crush is
// closed. When the AI asks for an action and Job Crush is closed, the answer
// says so in plain words, so the AI can tell the user to open it.
//
// It never touches the database or any file.

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalSocket>
#include <QSet>
#include <QThread>

#include <cstdio>
#include <iostream>
#include <string>

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

#include "modelview/aiapps/AiAppTools.h"

namespace {

// Writes one message to the AI app, as one line.
void sendToAiApp(const QJsonObject &message)
{
    const QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
    std::fflush(stdout);
}

void sendRawLineToAiApp(const QByteArray &line)
{
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

const QString jobCrushIsClosedText = QStringLiteral(
    "Job Crush is not open, so this could not be done. Ask the user to open Job Crush, "
    "then try again.");

// Reads standard input on its own thread. Reading it on the main thread would
// stop the bridge from hearing Job Crush while it waits for the AI app.
class StandardInputReader : public QThread {
    Q_OBJECT
signals:
    void lineRead(const QByteArray &line);
    void inputEnded();

protected:
    void run() override
    {
        std::string line;
        while (std::getline(std::cin, line)) {
            emit lineRead(QByteArray::fromStdString(line));
        }
        emit inputEnded();
    }
};

class Bridge : public QObject {
    Q_OBJECT
public:
    Bridge()
    {
        connect(&jobCrush, &QLocalSocket::readyRead, this, &Bridge::passAnswersBack);
        connect(&jobCrush, &QLocalSocket::disconnected, this, &Bridge::jobCrushWentAway);
    }

    void handleLineFromAiApp(const QByteArray &rawLine)
    {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty()) {
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            sendToAiApp(AiAppTools::errorMessage(QJsonValue(), AiAppTools::ParseError,
                                                 QStringLiteral("That message was not JSON.")));
            return;
        }

        const QJsonObject message = document.object();
        const QString method = message.value(QStringLiteral("method")).toString();
        const QJsonValue requestId = message.value(QStringLiteral("id"));
        if (requestId.isUndefined()) {
            return; // a notification; nothing to answer
        }

        if (method == QStringLiteral("initialize")) {
            sendToAiApp(AiAppTools::resultMessage(
                requestId,
                AiAppTools::initializeResult(message.value(QStringLiteral("params")).toObject()
                                                 .value(QStringLiteral("protocolVersion"))
                                                 .toString())));
            return;
        }
        if (method == QStringLiteral("ping")) {
            sendToAiApp(AiAppTools::resultMessage(requestId, {}));
            return;
        }
        if (method == QStringLiteral("tools/list")) {
            sendToAiApp(AiAppTools::resultMessage(
                requestId, { { QStringLiteral("tools"), AiAppTools::toolDefinitions() } }));
            return;
        }
        if (method != QStringLiteral("tools/call")) {
            sendToAiApp(AiAppTools::errorMessage(requestId, AiAppTools::MethodNotFound,
                                                 QStringLiteral("Job Crush does not know "
                                                                "\"%1\".").arg(method)));
            return;
        }

        // An action. Only Job Crush can do it.
        if (!reachJobCrush()) {
            sendToAiApp(AiAppTools::resultMessage(
                requestId, AiAppTools::toolCallResult(jobCrushIsClosedText, true)));
            return;
        }
        waitingForAnswers.insert(QJsonDocument(QJsonObject { { QStringLiteral("id"), requestId } })
                                     .toJson(QJsonDocument::Compact));
        jobCrush.write(line + '\n');
        jobCrush.flush();
    }

private:
    bool reachJobCrush()
    {
        if (jobCrush.state() == QLocalSocket::ConnectedState) {
            return true;
        }
        jobCrush.connectToServer(AiAppTools::ConnectionName);
        return jobCrush.waitForConnected(2000);
    }

    void passAnswersBack()
    {
        unfinishedLine += jobCrush.readAll();
        int lineEnd = unfinishedLine.indexOf('\n');
        while (lineEnd >= 0) {
            const QByteArray line = unfinishedLine.left(lineEnd).trimmed();
            unfinishedLine.remove(0, lineEnd + 1);
            lineEnd = unfinishedLine.indexOf('\n');
            if (line.isEmpty()) {
                continue;
            }
            const QJsonValue answeredId =
                QJsonDocument::fromJson(line).object().value(QStringLiteral("id"));
            waitingForAnswers.remove(
                QJsonDocument(QJsonObject { { QStringLiteral("id"), answeredId } })
                    .toJson(QJsonDocument::Compact));
            sendRawLineToAiApp(line);
        }
    }

    // Job Crush closed in the middle of an action. Every action still
    // waiting is answered, so the AI app is never left hanging.
    void jobCrushWentAway()
    {
        for (const QByteArray &waitingIdJson : std::as_const(waitingForAnswers)) {
            const QJsonValue requestId =
                QJsonDocument::fromJson(waitingIdJson).object().value(QStringLiteral("id"));
            sendToAiApp(AiAppTools::resultMessage(
                requestId, AiAppTools::toolCallResult(jobCrushIsClosedText, true)));
        }
        waitingForAnswers.clear();
        unfinishedLine.clear();
    }

    QLocalSocket jobCrush;
    QByteArray unfinishedLine;
    QSet<QByteArray> waitingForAnswers;
};

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

#ifdef Q_OS_WIN
    // Binary mode, so Windows does not turn each line ending into two
    // characters on the way out.
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    Bridge bridge;
    StandardInputReader inputReader;
    QObject::connect(&inputReader, &StandardInputReader::lineRead,
                     &bridge, &Bridge::handleLineFromAiApp, Qt::QueuedConnection);
    QObject::connect(&inputReader, &StandardInputReader::inputEnded,
                     &application, &QCoreApplication::quit, Qt::QueuedConnection);
    inputReader.start();

    const int exitCode = application.exec();
    // The reader is stuck waiting on input that will never come once the AI
    // app has gone. Nothing is left to do, so the program just ends.
    inputReader.terminate();
    inputReader.wait(500);
    return exitCode;
}

#include "main.moc"
