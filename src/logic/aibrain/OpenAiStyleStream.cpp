#include "OpenAiStyleStream.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include <memory>

#include "AiBrainReply.h"

namespace OpenAiStyleStream {

void readInto(QNetworkReply *networkReply, AiBrainReply *brainReply,
              const QString &rejectedKeyText)
{
    // A rolling buffer: cut complete lines, pass on every piece of text, and
    // keep a half-arrived line for the next read.
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

            if (!completeLine.startsWith("data:")) {
                continue; // comments and blank keep-alive lines
            }
            const QByteArray payloadJson = completeLine.mid(5).trimmed();
            if (payloadJson == QByteArrayLiteral("[DONE]")) {
                continue; // the network reply's finished signal closes out
            }

            const QJsonObject payloadObject =
                QJsonDocument::fromJson(payloadJson).object();

            // Mid-stream errors arrive as an "error" object.
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
