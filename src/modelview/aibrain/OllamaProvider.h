#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QString>

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
// Which model: the first one the user has downloaded into Ollama, read from
// Ollama's own list. A model choice in Settings arrives with item 6 (brain
// levels).
class OllamaProvider : public AiBrainProvider {
public:
    OllamaProvider();

    QString providerDisplayName() const override;

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
    void sendConversation(AiBrainReply *brainReply, const QByteArray &requestBody);
    QByteArray buildRequestBody(const QString &modelName, const QString &soulText,
                                const QList<AiBrainConversationMessage> &conversation) const;

    QNetworkAccessManager networkAccessManager;

    // The model the last list named, so a chat does not ask for the list
    // every time.
    QString knownModelName;
};
