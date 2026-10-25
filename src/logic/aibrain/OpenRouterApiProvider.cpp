#include "OpenRouterApiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "AiBrainReply.h"
#include "OpenAiStyleStream.h"

namespace {

// The OpenRouter chat-completions endpoint (OpenAI-compatible).
const QString openRouterEndpointUrl =
    QStringLiteral("https://openrouter.ai/api/v1/chat/completions");

// The connection check reads this instead: OpenRouter's key-info endpoint is
// authenticated, free, and tiny — the cheapest honest way to ask "does this
// key work?" without buying a completion to find out.
const QString openRouterKeyInfoEndpointUrl =
    QStringLiteral("https://openrouter.ai/api/v1/key");

// The model for each brain level. These are OpenRouter's "latest" names,
// which always point to the newest Claude model of that size, so this list
// does not go stale the way a pinned version would. Checked 2026-10-23 on
// openrouter.ai.
QString openRouterModelNameFor(AiBrainLevel brainLevel)
{
    switch (brainLevel) {
    case AiBrainLevel::Quick:    return QStringLiteral("~anthropic/claude-haiku-latest");
    case AiBrainLevel::Balanced: return QStringLiteral("~anthropic/claude-sonnet-latest");
    case AiBrainLevel::Best:     return QStringLiteral("~anthropic/claude-opus-latest");
    }
    return QStringLiteral("~anthropic/claude-sonnet-latest");
}

// Generous but bounded — chat answers and cover-letter drafts, not novels.
constexpr int openRouterMaxResponseTokens = 2048;

} // namespace

OpenRouterApiProvider::OpenRouterApiProvider() = default;

QString OpenRouterApiProvider::modelNameFor(AiBrainLevel brainLevel) const
{
    return openRouterModelNameFor(brainLevel);
}

QString OpenRouterApiProvider::providerDisplayName() const
{
    return QStringLiteral("OpenRouter");
}

QByteArray OpenRouterApiProvider::buildRequestBody(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiBrainRequestOptions &requestOptions) const
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
    requestBodyObject.insert(QStringLiteral("model"),
                             openRouterModelNameFor(requestOptions.brainLevel));
    requestBodyObject.insert(QStringLiteral("max_tokens"),
                             qMax(openRouterMaxResponseTokens, requestOptions.longestAnswerTokens));
    requestBodyObject.insert(QStringLiteral("stream"), true);
    requestBodyObject.insert(QStringLiteral("messages"), messagesArray);

    // OpenRouter's web search is a plugin named "web". It adds search results
    // to the prompt before the model answers, whatever model is picked, and
    // the answer arrives as ordinary content deltas.
    if (requestOptions.letTheBrainSearchTheWeb) {
        QJsonObject webPlugin;
        webPlugin.insert(QStringLiteral("id"), QStringLiteral("web"));
        QJsonArray pluginsArray;
        pluginsArray.append(webPlugin);
        requestBodyObject.insert(QStringLiteral("plugins"), pluginsArray);
    }

    return QJsonDocument(requestBodyObject).toJson(QJsonDocument::Compact);
}

AiBrainReply *OpenRouterApiProvider::streamConversation(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiCredential &credential,
    QObject *replyParent,
    const AiBrainRequestOptions &requestOptions)
{
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(openRouterEndpointUrl)};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("Authorization"),
                                QByteArrayLiteral("Bearer ") + credential.secretKey.toUtf8());
    // These two headers name the app to OpenRouter, for its public list of
    // apps. They are optional, and the request works without them.
    networkRequest.setRawHeader(QByteArrayLiteral("HTTP-Referer"),
                                QByteArrayLiteral("https://github.com/ultramanjones/jobcrush"));
    networkRequest.setRawHeader(QByteArrayLiteral("X-OpenRouter-Title"),
                                QByteArrayLiteral("Job Crush"));

    QNetworkReply *networkReply = networkAccessManager.post(
        networkRequest, buildRequestBody(soulText, conversation, requestOptions));
    networkReply->setParent(brainReply); // dies with the brain reply

    OpenAiStyleStream::readInto(
        networkReply, brainReply,
        QStringLiteral("OpenRouter did not accept this key. Double-check it in "
                       "Settings — or delete it there and add a fresh one from "
                       "OpenRouter's key page."));

    return brainReply;
}

AiBrainReply *OpenRouterApiProvider::verifyCredentialConnection(
    const AiCredential &credential, QObject *replyParent)
{
    // OpenRouter publishes a key-info endpoint: it costs nothing, returns the
    // key's own limits, and rejects a bad key outright. So it answers "is this
    // brain connected?" without spending a token.
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(openRouterKeyInfoEndpointUrl)};
    networkRequest.setRawHeader(QByteArrayLiteral("Authorization"),
                                QByteArrayLiteral("Bearer ") + credential.secretKey.toUtf8());

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
                QStringLiteral("OpenRouter would not accept that key."),
                QStringLiteral(
                    "Sign in at openrouter.ai, open Keys, and copy the key again "
                    "— OpenRouter keys begin with \"sk-or-\". Delete the old one "
                    "on the openrouter tab above and paste the new one in. Worth "
                    "checking the same page for a spent balance: OpenRouter turns "
                    "a key off when the credit runs out, and it reads exactly "
                    "like a bad key from here."));
            return;
        }
        brainReply->markFailed(
            networkReply->errorString(),
            QStringLiteral(
                "That looks like the connection rather than the key. Check that "
                "you are online and tick the box again. On a work laptop or a "
                "VPN, openrouter.ai is sometimes blocked outright — trying it in "
                "a browser will tell you in a few seconds."));
    });

    return brainReply;
}
