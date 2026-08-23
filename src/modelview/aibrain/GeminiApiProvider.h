#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>

#include "AiBrainProvider.h"

// GeminiApiProvider
//
// Google's Generative Language API. A third wire format after Anthropic's and
// OpenAI's — Google nests everything in "contents" and "parts", keeps the
// system prompt in its own field, and streams server-sent events only when
// asked for them with alt=sse.
//
// The key rides in a header rather than the query string Google's own
// examples use. A key in a URL ends up in proxy logs, browser history and
// crash reports; a key in a header does not.
class GeminiApiProvider : public AiBrainProvider {
public:
    GeminiApiProvider();

    QString providerDisplayName() const override;

    AiBrainReply *streamConversation(const QString &soulText,
                                     const QList<AiBrainConversationMessage> &conversation,
                                     const AiCredential &credential,
                                     QObject *replyParent) override;

    AiBrainReply *verifyCredentialConnection(const AiCredential &credential,
                                             QObject *replyParent) override;

private:
    QByteArray buildRequestBody(const QString &soulText,
                                const QList<AiBrainConversationMessage> &conversation) const;

    QNetworkAccessManager networkAccessManager;
};
