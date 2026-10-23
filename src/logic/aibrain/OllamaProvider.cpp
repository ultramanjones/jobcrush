#include "OllamaProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "AiBrainReply.h"
#include "OpenAiStyleStream.h"

namespace {

const QString ollamaChatEndpointUrl =
    QStringLiteral("http://localhost:11434/v1/chat/completions");
const QString ollamaModelListEndpointUrl = QStringLiteral("http://localhost:11434/api/tags");

const QString ollamaIsNotRunningText = QStringLiteral(
    "Job Crush could not reach Ollama on this computer.");
const QString startOllamaText = QStringLiteral(
    "Install Ollama from ollama.com if it is not installed, then start it. On "
    "Windows it runs from the Start menu and sits in the tray by the clock. "
    "Then tick the box again.");
const QString noModelText = QStringLiteral(
    "Ollama is running but has no AI model downloaded yet.");
const QString downloadAModelText = QStringLiteral(
    "Open a terminal and run: ollama pull llama3.2  It downloads once (about 2 "
    "GB). Then tick the box again.");

// The first model in Ollama's list, or empty when it has none.
QString firstModelNameIn(const QByteArray &modelListJson)
{
    const QJsonArray models = QJsonDocument::fromJson(modelListJson).object()
        .value(QStringLiteral("models")).toArray();
    if (models.isEmpty()) {
        return QString();
    }
    return models.first().toObject().value(QStringLiteral("name")).toString();
}

} // namespace

OllamaProvider::OllamaProvider() = default;

QString OllamaProvider::providerDisplayName() const
{
    return QStringLiteral("Ollama");
}

QByteArray OllamaProvider::buildRequestBody(
    const QString &modelName, const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation) const
{
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
    const QJsonObject requestBodyObject {
        { QStringLiteral("model"), modelName },
        { QStringLiteral("stream"), true },
        { QStringLiteral("messages"), messagesArray },
    };
    return QJsonDocument(requestBodyObject).toJson(QJsonDocument::Compact);
}

AiBrainReply *OllamaProvider::streamConversation(
    const QString &soulText,
    const QList<AiBrainConversationMessage> &conversation,
    const AiCredential &credential,
    QObject *replyParent,
    const AiBrainRequestOptions &requestOptions)
{
    Q_UNUSED(credential);       // Ollama has no key
    Q_UNUSED(requestOptions);   // a local model cannot search the web

    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    if (!knownModelName.isEmpty()) {
        sendConversation(brainReply, buildRequestBody(knownModelName, soulText, conversation));
        return brainReply;
    }

    // No model known yet: ask Ollama for its list first, then send.
    QNetworkReply *listReply =
        networkAccessManager.get(QNetworkRequest(QUrl(ollamaModelListEndpointUrl)));
    listReply->setParent(brainReply);
    QObject::connect(listReply, &QNetworkReply::finished, brainReply,
                     [this, listReply, brainReply, soulText, conversation]() {
        if (listReply->error() != QNetworkReply::NoError) {
            brainReply->markFailed(ollamaIsNotRunningText, startOllamaText);
            return;
        }
        knownModelName = firstModelNameIn(listReply->readAll());
        if (knownModelName.isEmpty()) {
            brainReply->markFailed(noModelText, downloadAModelText);
            return;
        }
        sendConversation(brainReply, buildRequestBody(knownModelName, soulText, conversation));
    });
    return brainReply;
}

void OllamaProvider::sendConversation(AiBrainReply *brainReply, const QByteArray &requestBody)
{
    QNetworkRequest networkRequest{QUrl(ollamaChatEndpointUrl)};
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/json"));
    QNetworkReply *networkReply = networkAccessManager.post(networkRequest, requestBody);
    networkReply->setParent(brainReply);
    OpenAiStyleStream::readInto(networkReply, brainReply, ollamaIsNotRunningText);
}

AiBrainReply *OllamaProvider::verifyCredentialConnection(const AiCredential &credential,
                                                         QObject *replyParent)
{
    Q_UNUSED(credential);
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    QNetworkReply *listReply =
        networkAccessManager.get(QNetworkRequest(QUrl(ollamaModelListEndpointUrl)));
    listReply->setParent(brainReply);
    QObject::connect(listReply, &QNetworkReply::finished, brainReply,
                     [this, listReply, brainReply]() {
        if (listReply->error() != QNetworkReply::NoError) {
            brainReply->markFailed(ollamaIsNotRunningText, startOllamaText);
            return;
        }
        knownModelName = firstModelNameIn(listReply->readAll());
        if (knownModelName.isEmpty()) {
            brainReply->markFailed(noModelText, downloadAModelText);
            return;
        }
        brainReply->markFinished();
    });
    return brainReply;
}
