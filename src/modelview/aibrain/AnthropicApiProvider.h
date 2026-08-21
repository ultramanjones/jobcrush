#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>

#include "AiBrainProvider.h"

// AnthropicApiProvider
//
// The Anthropic implementation of AiBrainProvider: a thin HTTPS client for
// the Messages API (POST /v1/messages) with streaming enabled. Fragments
// arrive as server-sent events and are forwarded to the AiBrainReply as they
// come in, so the chat shows words the moment they exist.
//
// This is exactly the "small, ordinary networking code" the plan calls for —
// one request shape, one response parser, nothing clever.
class AnthropicApiProvider : public AiBrainProvider {
public:
    AnthropicApiProvider();

    QString providerDisplayName() const override;

    AiBrainReply *streamConversation(const QString &soulText,
                                     const QList<AiBrainConversationMessage> &conversation,
                                     const AiCredential &credential,
                                     QObject *replyParent) override;

private:
    // Builds the JSON request body for the Messages API.
    QByteArray buildRequestBody(const QString &soulText,
                                const QList<AiBrainConversationMessage> &conversation) const;

    QNetworkAccessManager networkAccessManager;
};
