#pragma once

#include <QString>

class AiBrainReply;
class QNetworkReply;

// OpenAiStyleStream
//
// Reads a streamed chat answer in OpenAI's shape into an AiBrainReply.
//
// OpenRouter, OpenAI and Ollama all answer the same way, as server-sent
// events:
//
//     data: {"choices":[{"delta":{"content":"Hel"}}]}
//     data: [DONE]
//
// so the reading lives here once instead of in each provider.
namespace OpenAiStyleStream {

// Wires networkReply to brainReply: every piece of text is passed on as it
// arrives, and the end or a failure closes the brain reply.
//
// rejectedKeyText is what the user is told when the vendor refuses the key,
// because Qt throws the vendor's own words away on a 401.
void readInto(QNetworkReply *networkReply, AiBrainReply *brainReply,
              const QString &rejectedKeyText);

} // namespace OpenAiStyleStream
