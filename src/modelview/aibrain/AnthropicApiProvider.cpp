#include "AnthropicApiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <memory>

#include "AiBrainReply.h"

namespace {

// The Anthropic Messages API endpoint and protocol version.
const QString anthropicMessagesEndpointUrl = QStringLiteral("https://api.anthropic.com/v1/messages");

// The connection check reads this instead: listing models is authenticated,
// free, and small — the cheapest honest way to ask "does this key work?".
const QString anthropicModelsEndpointUrl = QStringLiteral("https://api.anthropic.com/v1/models");
const QByteArray anthropicApiVersionHeaderValue = QByteArrayLiteral("2023-06-01");

// The model Brain Chat speaks to. One named place to change it; a Settings
// choice can replace this constant in a later phase.
const QString anthropicDefaultModelName = QStringLiteral("claude-sonnet-4-5");

// Generous but bounded — chat answers and cover-letter drafts, not novels.
constexpr int anthropicMaxResponseTokens = 2048;

} // namespace

AnthropicApiProvider::AnthropicApiProvider() = default;

QString AnthropicApiProvider::providerDisplayName() const
{
    return QStringLiteral("Anthropic");
}

QByteArray AnthropicApiProvider::buildRequestBody(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation) const
{
    // Translate the vendor-neutral conversation into Anthropic's wire format:
    // the soul rides as the system prompt; turns alternate user/assistant.
    QJsonArray messagesArray;
    for (const AiBrainConversationMessage &conversationMessage : conversation) {
        QJsonObject messageObject;
        messageObject.insert(
            QStringLiteral("role"),
            conversationMessage.author == AiBrainConversationMessage::Author::Human
                ? QStringLiteral("user")
                : QStringLiteral("assistant"));
        messageObject.insert(QStringLiteral("content"), conversationMessage.messageText);
        messagesArray.append(messageObject);
    }

    QJsonObject requestBodyObject;
    requestBodyObject.insert(QStringLiteral("model"), anthropicDefaultModelName);
    requestBodyObject.insert(QStringLiteral("max_tokens"), anthropicMaxResponseTokens);
    requestBodyObject.insert(QStringLiteral("stream"), true);
    if (!soulText.isEmpty()) {
        requestBodyObject.insert(QStringLiteral("system"), soulText);
    }
    requestBodyObject.insert(QStringLiteral("messages"), messagesArray);

    return QJsonDocument(requestBodyObject).toJson(QJsonDocument::Compact);
}

AiBrainReply *AnthropicApiProvider::streamConversation(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiCredential &credential,
    QObject *replyParent)
{
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(anthropicMessagesEndpointUrl)};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("x-api-key"),
                                credential.secretKey.toUtf8());
    networkRequest.setRawHeader(QByteArrayLiteral("anthropic-version"),
                                anthropicApiVersionHeaderValue);

    QNetworkReply *networkReply = networkAccessManager.post(
        networkRequest, buildRequestBody(soulText, conversation));
    networkReply->setParent(brainReply); // dies with the brain reply

    // --- Server-sent event parsing --------------------------------------
    //
    // The stream arrives as SSE frames separated by blank lines:
    //
    //     event: content_block_delta
    //     data: {"type":"content_block_delta","delta":{"type":"text_delta","text":"Hel"}}
    //
    // We keep a rolling buffer per reply (captured by the lambda), cut off
    // complete frames as they arrive, and forward every text_delta fragment.
    // Anything half-received stays in the buffer for the next readyRead.
    auto streamParseBuffer = std::make_shared<QByteArray>();

    QObject::connect(networkReply, &QNetworkReply::readyRead, brainReply,
                     [networkReply, brainReply, streamParseBuffer]() {
        streamParseBuffer->append(networkReply->readAll());

        // Frames are separated by a blank line ("\n\n").
        int frameSeparatorIndex = streamParseBuffer->indexOf("\n\n");
        while (frameSeparatorIndex >= 0) {
            const QByteArray completeFrame = streamParseBuffer->left(frameSeparatorIndex);
            streamParseBuffer->remove(0, frameSeparatorIndex + 2);
            frameSeparatorIndex = streamParseBuffer->indexOf("\n\n");

            // Within a frame, only the "data:" lines carry JSON payloads.
            const QList<QByteArray> frameLines = completeFrame.split('\n');
            for (const QByteArray &frameLine : frameLines) {
                if (!frameLine.startsWith("data:")) {
                    continue;
                }
                const QByteArray payloadJson = frameLine.mid(5).trimmed();
                const QJsonObject payloadObject =
                    QJsonDocument::fromJson(payloadJson).object();
                const QString payloadType =
                    payloadObject.value(QStringLiteral("type")).toString();

                if (payloadType == QStringLiteral("content_block_delta")) {
                    const QJsonObject deltaObject =
                        payloadObject.value(QStringLiteral("delta")).toObject();
                    if (deltaObject.value(QStringLiteral("type")).toString()
                            == QStringLiteral("text_delta")) {
                        brainReply->appendStreamedFragment(
                            deltaObject.value(QStringLiteral("text")).toString());
                    }
                } else if (payloadType == QStringLiteral("error")) {
                    // The API can report an error mid-stream inside an event.
                    const QJsonObject errorObject =
                        payloadObject.value(QStringLiteral("error")).toObject();
                    brainReply->markFailed(
                        errorObject.value(QStringLiteral("message")).toString());
                }
            }
        }
    });

    QObject::connect(networkReply, &QNetworkReply::finished, brainReply,
                     [networkReply, brainReply]() {
        if (brainReply->isFinished()) {
            return; // already failed mid-stream; nothing more to say
        }

        if (networkReply->error() != QNetworkReply::NoError) {
            // A 401 makes Qt discard the body, so the vendor's own message is
            // usually gone — say something a human can act on instead.
            if (networkReply->error() == QNetworkReply::AuthenticationRequiredError) {
                brainReply->markFailed(QStringLiteral(
                    "Anthropic did not accept this key. Double-check it in "
                    "Settings — or delete it there and add a fresh one from "
                    "Anthropic's key page."));
                return;
            }
            // Other error responses (overloaded, no network) arrive as one
            // JSON body; surface Anthropic's own message when there is one,
            // the network error text otherwise.
            const QJsonObject errorBodyObject =
                QJsonDocument::fromJson(networkReply->readAll()).object();
            const QString apiErrorMessage = errorBodyObject
                .value(QStringLiteral("error")).toObject()
                .value(QStringLiteral("message")).toString();
            brainReply->markFailed(apiErrorMessage.isEmpty()
                                       ? networkReply->errorString()
                                       : apiErrorMessage);
            return;
        }

        brainReply->markFinished();
    });

    return brainReply;
}

AiBrainReply *AnthropicApiProvider::verifyCredentialConnection(
    const AiCredential &credential, QObject *replyParent)
{
    // The cheapest authenticated read Anthropic offers: list the models.
    // It costs no tokens, returns a small body, and fails loudly on a bad
    // key — exactly what "is this brain actually connected?" needs.
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(anthropicModelsEndpointUrl)};
    networkRequest.setRawHeader(QByteArrayLiteral("x-api-key"),
                                credential.secretKey.toUtf8());
    networkRequest.setRawHeader(QByteArrayLiteral("anthropic-version"),
                                anthropicApiVersionHeaderValue);

    QNetworkReply *networkReply = networkAccessManager.get(networkRequest);
    networkReply->setParent(brainReply); // dies with the brain reply

    QObject::connect(networkReply, &QNetworkReply::finished, brainReply,
                     [networkReply, brainReply]() {
        if (networkReply->error() == QNetworkReply::NoError) {
            brainReply->markFinished();
            return;
        }
        if (networkReply->error() == QNetworkReply::AuthenticationRequiredError) {
            brainReply->markFailed(
                QStringLiteral("Anthropic would not accept that key."),
                QStringLiteral(
                    "Sign in at console.anthropic.com, open API keys, and copy "
                    "the key again — Anthropic keys begin with \"sk-ant-\". "
                    "Delete the old one on the anthropic tab above and paste the "
                    "new one in. If you regenerated the key anywhere else, every "
                    "older copy of it stopped working the moment you did."));
            return;
        }
        // Not a key problem — say so, so nobody spends the evening replacing a
        // key that was fine all along.
        brainReply->markFailed(
            networkReply->errorString(),
            QStringLiteral(
                "That looks like the connection rather than the key. Check that "
                "you are online and tick the box again. On a work laptop or a "
                "VPN, api.anthropic.com is sometimes blocked outright — trying "
                "it in a browser will tell you in a few seconds."));
    });

    return brainReply;
}
