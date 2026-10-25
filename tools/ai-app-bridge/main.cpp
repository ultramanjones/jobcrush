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
// It never touches the database or any file. Its log lines go to standard
// error, because standard output belongs to the AI app.

#include <QCoreApplication>
#include <QDebug>
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

#include "logic/aiapps/AiAppTools.h"

namespace {

// How long the bridge waits for Job Crush to accept a connection. Job Crush
// is on the same computer, so two seconds is plenty when it is open.
constexpr int jobCrushConnectWaitMilliseconds = 2000;

// How long main waits for the input thread to stop after it is told to.
constexpr int inputReaderStopWaitMilliseconds = 500;

// Writes one message to the AI app, as one line.
void sendToAiApp(const QJsonObject &message)
{
    const QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
    std::fflush(stdout);
}

// Writes a line Job Crush already wrote as JSON, unchanged, plus a line end.
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

// Bridge
//
// Answers what it can by itself and passes each action on to Job Crush.
// It keeps the id of every action still waiting, so that if Job Crush closes
// part way, each one still gets an answer.
class Bridge : public QObject {
    Q_OBJECT
public:
    Bridge()
    {
        connect(&jobCrush, &QLocalSocket::readyRead, this, &Bridge::passAnswersBack);
        connect(&jobCrush, &QLocalSocket::disconnected, this, &Bridge::jobCrushWentAway);
    }

    // Handles one line from the AI app. initialize, ping and tools/list are
    // answered here. tools/call goes to Job Crush. Anything else is an error.
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
        // The id is kept as the compact JSON text {"id":...}. A JSON-RPC id
        // can be a number or a string, and as text both compare correctly.
        waitingForAnswers.insert(QJsonDocument(QJsonObject { { QStringLiteral("id"), requestId } })
                                     .toJson(QJsonDocument::Compact));
        jobCrush.write(line + '\n');
        jobCrush.flush();
    }

private:
    // Connects to Job Crush if not connected yet. Returns false when Job
    // Crush is not open.
    bool reachJobCrush()
    {
        if (jobCrush.state() == QLocalSocket::ConnectedState) {
            return true;
        }
        jobCrush.connectToServer(AiAppTools::ConnectionName);
        if (jobCrush.waitForConnected(jobCrushConnectWaitMilliseconds)) {
            return true;
        }
        qWarning().noquote() << QStringLiteral(
            "Bridge::reachJobCrush: could not connect to \"%1\" — %2")
            .arg(AiAppTools::ConnectionName, jobCrush.errorString());
        return false;
    }

    // Passes each whole line Job Crush wrote on to the AI app, and crosses
    // its id off the waiting list. Half a line waits for the next read.
    void passAnswersBack()
    {
        unfinishedLine += jobCrush.readAll();
        int lineBreakIndex = unfinishedLine.indexOf('\n');
        while (lineBreakIndex >= 0) {
            const QByteArray line = unfinishedLine.left(lineBreakIndex).trimmed();
            unfinishedLine.remove(0, lineBreakIndex + 1);
            lineBreakIndex = unfinishedLine.indexOf('\n');
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

    // Bytes from Job Crush that do not yet make a whole line.
    QByteArray unfinishedLine;

    // The ids of actions sent to Job Crush and not answered yet.
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
    // The reader may be stuck waiting on input that will never come once the
    // AI app has gone. Nothing is left to do, so the thread is stopped and
    // the program ends.
    inputReader.terminate();
    inputReader.wait(inputReaderStopWaitMilliseconds);
    return exitCode;
}

#include "main.moc"
