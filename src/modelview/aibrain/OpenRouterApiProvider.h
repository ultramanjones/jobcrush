#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>

#include "AiBrainProvider.h"

// OpenRouterApiProvider
//
// The OpenRouter implementation of AiBrainProvider: one key, many models —
// OpenRouter fronts Anthropic, OpenAI, Google, Meta and friends behind a
// single OpenAI-compatible chat-completions API. Streaming arrives as
// OpenAI-style server-sent events.
//
// This client is deliberately the dress rehearsal for the OpenAI provider:
// the wire format is the same, so when OpenAiApiProvider lands it will be
// this file with a different endpoint and default model.
class OpenRouterApiProvider : public AiBrainProvider {
public:
    OpenRouterApiProvider();

    QString providerDisplayName() const override;

    AiBrainReply *streamConversation(const QString &soulText,
                                     const QList<AiBrainConversationMessage> &conversation,
                                     const AiCredential &credential,
                                     QObject *replyParent) override;

    AiBrainReply *verifyCredentialConnection(const AiCredential &credential,
                                             QObject *replyParent) override;

private:
    // Builds the OpenAI-style JSON request body (system message first).
    QByteArray buildRequestBody(const QString &soulText,
                                const QList<AiBrainConversationMessage> &conversation) const;

    QNetworkAccessManager networkAccessManager;
};
