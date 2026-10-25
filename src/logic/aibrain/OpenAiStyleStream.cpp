#include "OpenAiStyleStream.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include <memory>

#include "AiBrainReply.h"

namespace {

// Each payload line in the stream starts with this.
const QByteArray serverSentEventDataPrefix = QByteArrayLiteral("data:");

// The last payload line says only this. It carries no text.
const QByteArray endOfStreamPayload = QByteArrayLiteral("[DONE]");

} // namespace

namespace OpenAiStyleStream {

void readInto(QNetworkReply *networkReply, AiBrainReply *brainReply,
              const QString &rejectedKeyText)
{
    // A rolling buffer, one per reply, held by the lambda. The steps:
    //  1. Add whatever bytes arrived to the end of the buffer.
    //  2. While the buffer holds a line break, cut off one complete line.
    //  3. Skip lines that do not start with "data:" (comments, keep-alives)
    //     and the closing "data: [DONE]".
    //  4. Read the rest of the line as JSON. An "error" fails the reply.
    //     Otherwise the text in choices[0].delta.content is passed on.
    //  5. Bytes after the last line break are half a line. They stay in the
    //     buffer until the next readyRead brings the rest.
    auto streamParseBuffer = std::make_shared<QByteArray>();

    QObject::connect(networkReply, &QNetworkReply::readyRead, brainReply,
                     [networkReply, brainReply, streamParseBuffer]() {
        streamParseBuffer->append(networkReply->readAll());

        int lineBreakIndex = streamParseBuffer->indexOf('\n');
        while (lineBreakIndex >= 0) {
            const QByteArray completeLine =
                streamParseBuffer->left(lineBreakIndex).trimmed();
            streamParseBuffer->remove(0, lineBreakIndex + 1);
            lineBreakIndex = streamParseBuffer->indexOf('\n');

            if (!completeLine.startsWith(serverSentEventDataPrefix)) {
                continue; // comments and blank keep-alive lines
            }
            const QByteArray payloadJson =
                completeLine.mid(serverSentEventDataPrefix.size()).trimmed();
            if (payloadJson == endOfStreamPayload) {
                continue; // the network reply's finished signal closes out
            }

            const QJsonObject payloadObject =
                QJsonDocument::fromJson(payloadJson).object();

            // Mid-stream errors arrive as an "error" field. Some vendors send
            // an object with a message, others send plain text.
            if (payloadObject.contains(QStringLiteral("error"))) {
                const QJsonValue errorValue = payloadObject.value(QStringLiteral("error"));
                const QString errorText = errorValue.isObject()
                    ? errorValue.toObject().value(QStringLiteral("message")).toString()
                    : errorValue.toString();
                brainReply->markFailed(errorText);
                continue;
            }

            const QString contentDelta = payloadObject
                .value(QStringLiteral("choices")).toArray()
                .at(0).toObject()
                .value(QStringLiteral("delta")).toObject()
                .value(QStringLiteral("content")).toString();
            if (!contentDelta.isEmpty()) {
                brainReply->appendStreamedFragment(contentDelta);
            }
        }
    });

    QObject::connect(networkReply, &QNetworkReply::finished, brainReply,
                     [networkReply, brainReply, rejectedKeyText]() {
        if (brainReply->isFinished()) {
            return; // already failed mid-stream; nothing more to say
        }

        if (networkReply->error() != QNetworkReply::NoError) {
            // A 401 makes Qt discard the body, so the vendor's own message is
            // usually gone. Say something a person can act on instead.
            if (networkReply->error() == QNetworkReply::AuthenticationRequiredError) {
                brainReply->markFailed(rejectedKeyText);
                return;
            }
            const QJsonObject errorBodyObject =
                QJsonDocument::fromJson(networkReply->readAll()).object();
            const QJsonValue errorValue = errorBodyObject.value(QStringLiteral("error"));
            const QString apiErrorMessage = errorValue.isObject()
                ? errorValue.toObject().value(QStringLiteral("message")).toString()
                : errorValue.toString();
            brainReply->markFailed(apiErrorMessage.isEmpty()
                                       ? networkReply->errorString()
                                       : apiErrorMessage);
            return;
        }

        brainReply->markFinished();
    });
}

} // namespace OpenAiStyleStream
