#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>

#include "AiBrainProvider.h"

// OpenAiApiProvider
//
// The OpenAI implementation of AiBrainProvider, using an API key from
// platform.openai.com. Speaks OpenAI's chat completions API; the streamed
// answer is read by OpenAiStyleStream, the same reader OpenRouter uses.
//
// "Sign in with ChatGPT" is a different thing (BACKLOG item 4) and is still a
// closed program at OpenAI.
class OpenAiApiProvider : public AiBrainProvider {
public:
    OpenAiApiProvider();

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
    // Builds the JSON request body for chat completions (system message
    // first). Asking for a web search swaps in OpenAI's search model.
    QByteArray buildRequestBody(const QString &soulText,
                                const QList<AiBrainConversationMessage> &conversation,
                                const AiBrainRequestOptions &requestOptions) const;

    QNetworkAccessManager networkAccessManager;
};
