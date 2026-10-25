#include "OllamaProvider.h"

#include <algorithm>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "AiBrainReply.h"
#include "OpenAiStyleStream.h"

namespace {

// Ollama listens on this computer only. The chat endpoint speaks OpenAI's
// shape; the model list is Ollama's own.
const QString ollamaChatEndpointUrl =
    QStringLiteral("http://localhost:11434/v1/chat/completions");
const QString ollamaModelListEndpointUrl = QStringLiteral("http://localhost:11434/api/tags");

// What the user is told, as a reason and a next step, when Ollama cannot
// answer.
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

// The names of the models in Ollama's list, smallest download first. Empty
// when it has none. The size on disk is how the brain levels are told apart
// on a user's own computer: a bigger model is slower and usually smarter.
QStringList modelNamesSmallestFirstIn(const QByteArray &modelListJson)
{
    const QJsonArray models = QJsonDocument::fromJson(modelListJson).object()
        .value(QStringLiteral("models")).toArray();
    QList<QJsonObject> modelObjects;
    for (const QJsonValue &modelValue : models) {
        modelObjects.append(modelValue.toObject());
    }
    std::sort(modelObjects.begin(), modelObjects.end(),
              [](const QJsonObject &leftModel, const QJsonObject &rightModel) {
        return leftModel.value(QStringLiteral("size")).toDouble()
               < rightModel.value(QStringLiteral("size")).toDouble();
    });
    QStringList modelNames;
    for (const QJsonObject &modelObject : modelObjects) {
        modelNames.append(modelObject.value(QStringLiteral("name")).toString());
    }
    return modelNames;
}

} // namespace

OllamaProvider::OllamaProvider() = default;

QString OllamaProvider::modelNameFor(AiBrainLevel brainLevel) const
{
    // Before Ollama's list has been read, there are no names to give. The
    // Settings page shows a plain description instead.
    if (knownModelNamesSmallestFirst.isEmpty()) {
        switch (brainLevel) {
        case AiBrainLevel::Quick:    return QStringLiteral("your smallest downloaded model");
        case AiBrainLevel::Balanced: return QStringLiteral("a middle-sized downloaded model");
        case AiBrainLevel::Best:     return QStringLiteral("your largest downloaded model");
        }
    }
    switch (brainLevel) {
    case AiBrainLevel::Quick:
        return knownModelNamesSmallestFirst.first();
    case AiBrainLevel::Balanced:
        // The middle of the list. With an even count it takes the smaller of
        // the two middle models: 4 models gives index 1, 5 models gives 2.
        return knownModelNamesSmallestFirst.at((knownModelNamesSmallestFirst.count() - 1) / 2);
    case AiBrainLevel::Best:
        return knownModelNamesSmallestFirst.last();
    }
    return knownModelNamesSmallestFirst.first();
}

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
    // Ollama has no key. A local model cannot search the web, so
    // letTheBrainSearchTheWeb is ignored here, and so is longestAnswerTokens.
    Q_UNUSED(credential);

    AiBrainReply *brainReply = new AiBrainReply(replyParent);
    const AiBrainLevel brainLevel = requestOptions.brainLevel;

    if (!knownModelNamesSmallestFirst.isEmpty()) {
        sendConversation(brainReply, buildRequestBody(modelNameFor(brainLevel), soulText,
                                                      conversation));
        return brainReply;
    }

    // No model known yet: ask Ollama for its list first, then send.
    QNetworkReply *listReply =
        networkAccessManager.get(QNetworkRequest(QUrl(ollamaModelListEndpointUrl)));
    listReply->setParent(brainReply);
    QObject::connect(listReply, &QNetworkReply::finished, brainReply,
                     [this, listReply, brainReply, soulText, conversation, brainLevel]() {
        if (listReply->error() != QNetworkReply::NoError) {
            qWarning().noquote() << QStringLiteral(
                "OllamaProvider::streamConversation: could not read the model list at %1 — %2")
                .arg(ollamaModelListEndpointUrl, listReply->errorString());
            brainReply->markFailed(ollamaIsNotRunningText, startOllamaText);
            return;
        }
        knownModelNamesSmallestFirst = modelNamesSmallestFirstIn(listReply->readAll());
        if (knownModelNamesSmallestFirst.isEmpty()) {
            brainReply->markFailed(noModelText, downloadAModelText);
            return;
        }
        sendConversation(brainReply, buildRequestBody(modelNameFor(brainLevel), soulText,
                                                      conversation));
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
    Q_UNUSED(credential); // Ollama has no key
    AiBrainReply *brainReply = new AiBrainReply(replyParent);

    // Reading the list both checks that Ollama is running and remembers the
    // model names for the next chat.

    QNetworkReply *listReply =
        networkAccessManager.get(QNetworkRequest(QUrl(ollamaModelListEndpointUrl)));
    listReply->setParent(brainReply);
    QObject::connect(listReply, &QNetworkReply::finished, brainReply,
                     [this, listReply, brainReply]() {
        if (listReply->error() != QNetworkReply::NoError) {
            brainReply->markFailed(ollamaIsNotRunningText, startOllamaText);
            return;
        }
        knownModelNamesSmallestFirst = modelNamesSmallestFirstIn(listReply->readAll());
        if (knownModelNamesSmallestFirst.isEmpty()) {
            brainReply->markFailed(noModelText, downloadAModelText);
            return;
        }
        brainReply->markFinished();
    });
    return brainReply;
}
