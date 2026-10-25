#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>
#include <QStringList>

#include "AiBrainProvider.h"

// OllamaProvider
//
// Ollama runs AI models on the user's own computer. It needs no key, costs
// nothing per question, and nothing leaves the machine. It is slower than the
// paid brains and cannot search the web.
//
// Ollama listens on http://localhost:11434 and answers chat in OpenAI's shape,
// so the streamed answer is read by OpenAiStyleStream.
//
// Which model: Ollama's own list of the models the user has downloaded,
// sorted by size. Quick uses the smallest, Best the largest, and Balanced
// the one in the middle.
class OllamaProvider : public AiBrainProvider {
public:
    OllamaProvider();

    QString providerDisplayName() const override;
    QString modelNameFor(AiBrainLevel brainLevel) const override;

    AiBrainReply *streamConversation(const QString &soulText,
                                     const QList<AiBrainConversationMessage> &conversation,
                                     const AiCredential &credential,
                                     QObject *replyParent,
                                     const AiBrainRequestOptions &requestOptions
                                         = AiBrainRequestOptions()) override;

    // Asks Ollama for its list of downloaded models. Connected means Ollama
    // is running and has at least one.
    AiBrainReply *verifyCredentialConnection(const AiCredential &credential,
                                             QObject *replyParent) override;

private:
    // Posts one chat request and wires its stream into brainReply.
    void sendConversation(AiBrainReply *brainReply, const QByteArray &requestBody);

    // Builds the OpenAI-style JSON request body (system message first).
    QByteArray buildRequestBody(const QString &modelName, const QString &soulText,
                                const QList<AiBrainConversationMessage> &conversation) const;

    QNetworkAccessManager networkAccessManager;

    // The models the last list named, smallest first, so a chat does not ask
    // for the list every time. Quick uses the smallest, Best the largest.
    QStringList knownModelNamesSmallestFirst;
};
