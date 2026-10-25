#include "OpenAiApiProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "AiBrainReply.h"
#include "OpenAiStyleStream.h"

namespace {

const QString openAiChatEndpointUrl =
    QStringLiteral("https://api.openai.com/v1/chat/completions");

// The connection check reads the model list. It needs a working key, costs
// nothing, and buys no answer.
const QString openAiModelListEndpointUrl = QStringLiteral("https://api.openai.com/v1/models");

// The model for each brain level. Checked 2026-10-23: gpt-5-mini, the old
// default, shuts down 2026-12-11, and OpenAI names gpt-5.6-terra as its
// replacement.
QString openAiModelNameFor(AiBrainLevel brainLevel)
{
    switch (brainLevel) {
    case AiBrainLevel::Quick:    return QStringLiteral("gpt-5.6-luna");
    case AiBrainLevel::Balanced: return QStringLiteral("gpt-5.6-terra");
    case AiBrainLevel::Best:     return QStringLiteral("gpt-5.6-sol");
    }
    return QStringLiteral("gpt-5.6-terra");
}

// OpenAI's chat completions API only searches the web with this model, and
// only when web_search_options is in the request. Checked in OpenAI's web
// search guide on 2026-10-16; the older search-preview models were shut down
// on 2026-07-23.
const QString openAiWebSearchModelName = QStringLiteral("gpt-5-search-api");

// Generous but bounded. The newer models count their own thinking inside
// this number, so it is set higher than the other providers' limit.
constexpr int openAiMaxCompletionTokens = 4096;

} // namespace

OpenAiApiProvider::OpenAiApiProvider() = default;

QString OpenAiApiProvider::modelNameFor(AiBrainLevel brainLevel) const
{
    return openAiModelNameFor(brainLevel);
}

QString OpenAiApiProvider::providerDisplayName() const
{
    return QStringLiteral("OpenAI");
}

QByteArray OpenAiApiProvider::buildRequestBody(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiBrainRequestOptions &requestOptions) const
{
    // The soul goes first as a system message; the turns follow it.
    QJsonArray messagesArray;
    if (!soulText.isEmpty()) {
        messagesArray.append(QJsonObject { { QStringLiteral("role"), QStringLiteral("system") },
                                           { QStringLiteral("content"), soulText } });
    }
    for (const AiBrainConversationMessage &conversationMessage : conversation) {
        messagesArray.append(QJsonObject {
            { QStringLiteral("role"),
              conversationMessage.author == AiBrainConversationMessage::Author::Human
                  ? QStringLiteral("user")
                  : QStringLiteral("assistant") },
            { QStringLiteral("content"), conversationMessage.messageText } });
    }

    QJsonObject requestBodyObject;
    requestBodyObject.insert(QStringLiteral("model"),
                             requestOptions.letTheBrainSearchTheWeb
                                 ? openAiWebSearchModelName
                                 : openAiModelNameFor(requestOptions.brainLevel));
    // The newer models refuse "max_tokens" and want this name instead.
    requestBodyObject.insert(QStringLiteral("max_completion_tokens"),
                             qMax(openAiMaxCompletionTokens, requestOptions.longestAnswerTokens));
    requestBodyObject.insert(QStringLiteral("stream"), true);
    requestBodyObject.insert(QStringLiteral("messages"), messagesArray);
    if (requestOptions.letTheBrainSearchTheWeb) {
        requestBodyObject.insert(QStringLiteral("web_search_options"), QJsonObject {});
    }
    return QJsonDocument(requestBodyObject).toJson(QJsonDocument::Compact);
}

AiBrainReply *OpenAiApiProvider::streamConversation(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiCredential &credential,
    QObject *replyParent,
    const AiBrainRequestOptions &requestOptions)
{
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(openAiChatEndpointUrl)};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    networkRequest.setRawHeader(QByteArrayLiteral("Authorization"),
                                QByteArrayLiteral("Bearer ") + credential.secretKey.toUtf8());

    QNetworkReply *networkReply = networkAccessManager.post(
        networkRequest, buildRequestBody(soulText, conversation, requestOptions));
    networkReply->setParent(brainReply); // dies with the brain reply

    OpenAiStyleStream::readInto(
        networkReply, brainReply,
        QStringLiteral("OpenAI did not accept this key. Delete it in Settings and add a "
                       "fresh one from platform.openai.com, under API keys."));
    return brainReply;
}

AiBrainReply *OpenAiApiProvider::verifyCredentialConnection(const AiCredential &credential,
                                                            QObject *replyParent)
{
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkRequest networkRequest{QUrl(openAiModelListEndpointUrl)};
    networkRequest.setRawHeader(QByteArrayLiteral("Authorization"),
                                QByteArrayLiteral("Bearer ") + credential.secretKey.toUtf8());

    QNetworkReply *networkReply = networkAccessManager.get(networkRequest);
    networkReply->setParent(brainReply);

    QObject::connect(networkReply, &QNetworkReply::finished, brainReply,
                     [networkReply, brainReply]() {
        if (networkReply->error() == QNetworkReply::NoError) {
            brainReply->markFinished();
            return;
        }
        if (networkReply->error() == QNetworkReply::AuthenticationRequiredError) {
            brainReply->markFailed(
                QStringLiteral("OpenAI would not accept that key."),
                QStringLiteral(
                    "Sign in at platform.openai.com, open API keys, and make a new "
                    "key. OpenAI keys begin with \"sk-\". Delete the old one on the "
                    "openai tab above and paste the new one in. A ChatGPT Plus "
                    "subscription does not include API use; the API is paid "
                    "separately, under Billing on the same site."));
            return;
        }
        brainReply->markFailed(
            networkReply->errorString(),
            QStringLiteral(
                "That looks like the connection rather than the key. Check that you "
                "are online and tick the box again."));
    });
    return brainReply;
}
