#include "OpenRouterApiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <memory>

#include "AiBrainReply.h"

namespace {

// The OpenRouter chat-completions endpoint (OpenAI-compatible).
const QString openRouterEndpointUrl =
    QStringLiteral("https://openrouter.ai/api/v1/chat/completions");

// "openrouter/auto" lets OpenRouter pick a capable model — true
// set-it-and-forget-it until the per-provider model picker lands, at which
// point this constant becomes a Settings choice like the others.
const QString openRouterDefaultModelName = QStringLiteral("openrouter/auto");

// Generous but bounded — chat answers and cover-letter drafts, not novels.
constexpr int openRouterMaxResponseTokens = 2048;

} // namespace

OpenRouterApiProvider::OpenRouterApiProvider() = default;

QString OpenRouterApiProvider::providerDisplayName() const
{
    return QStringLiteral("OpenRouter");
}

QByteArray OpenRouterApiProvider::buildRequestBody(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation) const
{
    // OpenAI wire format: the soul rides as a leading system message;
    // turns alternate user/assistant after it.
    QJsonArray messagesArray;

    if (!soulText.isEmpty()) {
        QJsonObject systemMessageObject;
        systemMessageObject.insert(QStringLiteral("role"), QStringLiteral("system"));
        systemMessageObject.insert(QStringLiteral("content"), soulText);
        messagesArray.append(systemMessageObject);
    }

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
    requestBodyObject.insert(QStringLiteral("model"), openRouterDefaultModelName);
    requestBodyObject.insert(QStringLiteral("max_tokens"), openRouterMaxResponseTokens);
    requestBodyObject.insert(QStringLiteral("stream"), true);
    requestBodyObject.insert(QStringLiteral("messages"), messagesArray);

    return QJsonDocument(requestBodyObject).toJson(QJsonDocument::Compact);
}

AiBrainReply *OpenRouterApiProvider::streamConversation(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiCredential &credential,
    QObject *replyParent)
{
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(openRouterEndpointUrl)};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("Authorization"),
                                QByteArrayLiteral("Bearer ") + credential.secretKey.toUtf8());
    // App attribution (optional, for OpenRouter's public app rankings).
    networkRequest.setRawHeader(QByteArrayLiteral("HTTP-Referer"),
                                QByteArrayLiteral("https://github.com/ultramanjones/jobcrush"));
    networkRequest.setRawHeader(QByteArrayLiteral("X-OpenRouter-Title"),
                                QByteArrayLiteral("Job Crush"));

    QNetworkReply *networkReply = networkAccessManager.post(
        networkRequest, buildRequestBody(soulText, conversation));
    networkReply->setParent(brainReply); // dies with the brain reply

    // --- OpenAI-style SSE parsing ----------------------------------------
    //
    //     data: {"choices":[{"delta":{"content":"Hel"}}]}
    //     data: [DONE]
    //
    // Same rolling-buffer technique as the Anthropic provider: cut complete
    // lines, forward every content delta, keep partial lines for later.
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
                brainReply->markFailed(payloadObject
                    .value(QStringLiteral("error")).toObject()
                    .value(QStringLiteral("message")).toString());
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
                     [networkReply, brainReply]() {
        if (brainReply->isFinished()) {
            return; // already failed mid-stream; nothing more to say
        }

        if (networkReply->error() != QNetworkReply::NoError) {
            // A 401 makes Qt discard the body, so the vendor's own message is
            // usually gone — say something a human can act on instead.
            if (networkReply->error() == QNetworkReply::AuthenticationRequiredError) {
                brainReply->markFailed(QStringLiteral(
                    "OpenRouter did not accept this key. Double-check it in "
                    "Settings — or delete it there and add a fresh one from "
                    "OpenRouter's key page."));
                return;
            }
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
