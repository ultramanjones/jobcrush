#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>

#include "AiBrainProvider.h"

// OpenRouterApiProvider
//
// The OpenRouter implementation of AiBrainProvider: one key, many models.
// OpenRouter sells models from Anthropic, OpenAI, Google, Meta and others
// through one chat API in OpenAI's shape. The streamed answer is read by
// OpenAiStyleStream, the same reader OpenAiApiProvider and OllamaProvider use.
class OpenRouterApiProvider : public AiBrainProvider {
public:
    OpenRouterApiProvider();

    QString providerDisplayName() const override;
    QString modelNameFor(AiBrainLevel brainLevel) const override;

    AiBrainReply *streamConversation(const QString &soulText,
                                     const QList<AiBrainConversationMessage> &conversation,
                                     const AiCredential &credential,
                                     QObject *replyParent,
                                     const AiBrainRequestOptions &requestOptions
                                         = AiBrainRequestOptions()) override;

    AiBrainReply *verifyCredentialConnection(const AiCredential &credential,
                                             QObject *replyParent) override;

private:
    // Builds the OpenAI-style JSON request body (system message first).
    QByteArray buildRequestBody(const QString &soulText,
                                const QList<AiBrainConversationMessage> &conversation,
                                const AiBrainRequestOptions &requestOptions) const;

    QNetworkAccessManager networkAccessManager;
};
