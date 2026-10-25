#pragma once

#include <QList>
#include <QString>

#include "AiBrainLevel.h"
#include "AiCredential.h"

class AiBrainReply;
class QObject;

// AiBrainConversationMessage
//
// One turn of a conversation as providers see it: who spoke, and what they
// said. It belongs to no vendor on purpose. Each provider translates it into
// its own vendor's JSON.
struct AiBrainConversationMessage {
    enum class Author {
        Human,      // the user
        Brain       // a previous AIBrain response, sent back for context
    };

    Author author = Author::Human;
    QString messageText;
};

// AiBrainRequestOptions
//
// The few switches a caller can set on one request. Plain chat sets none of
// them.
//
// letTheBrainSearchTheWeb turns on the vendor's own web search tool for this
// one request. Each vendor has one (Anthropic, Google and OpenRouter all do)
// and each spells it differently, so the provider translates. It is off by
// default because a search costs extra and a chat message rarely needs one.
struct AiBrainRequestOptions {
    bool letTheBrainSearchTheWeb = false;

    // How long the answer may be, in tokens (a token is about 3/4 of a word).
    // 0 means the provider's usual limit, which is sized for chat. A job that
    // writes a whole document back, like laying out a resume, asks for more,
    // or the answer is cut off in the middle.
    int longestAnswerTokens = 0;

    // Which of the provider's models to use: Quick, Balanced or Best. AiBrain
    // fills this in from the user's choice for the provider.
    AiBrainLevel brainLevel = AiBrainLevel::Balanced;
};

// AiBrainProvider
//
// The interface every concrete provider implements: Anthropic, OpenRouter,
// OpenAI, Gemini and Ollama. Any one can stand in for another. A provider is
// a thin HTTP client
// speaking one vendor's JSON API: small, ordinary networking code. It holds
// no conversation state and no credentials of its own — everything it needs
// arrives with the call.
class AiBrainProvider {
public:
    virtual ~AiBrainProvider() = default;

    // The vendor name for logs and the UI, e.g. "Anthropic".
    virtual QString providerDisplayName() const = 0;

    // The vendor's model name for a level, for example "claude-sonnet-5-5"
    // for Balanced. Shown small under the level picker, and sent with every
    // request.
    virtual QString modelNameFor(AiBrainLevel brainLevel) const = 0;

    // Starts a streaming request and returns immediately. The returned reply
    // (parented to replyParent) delivers fragments, then finished or failed.
    //
    // soulText   — the agent's identity/prime-directives, sent as the system
    //              prompt (see AiBrainSoul).
    // conversation — the full turn history, oldest first.
    virtual AiBrainReply *streamConversation(const QString &soulText,
                                             const QList<AiBrainConversationMessage> &conversation,
                                             const AiCredential &credential,
                                             QObject *replyParent,
                                             const AiBrainRequestOptions &requestOptions
                                                 = AiBrainRequestOptions()) = 0;

    // Asks the vendor, as cheaply as possible, whether this credential
    // actually works right now — the plumbing behind "connected and active".
    // Each provider picks the smallest authenticated request its API offers
    // (a key-info or model-list read, never a paid completion), so checking
    // costs the user nothing.
    //
    // Reuses AiBrainReply for its shape and nothing else: finished() means
    // the vendor accepted the credential, failed() carries a plain-speech
    // reason. No text fragments arrive. Verification runs ONLY at key
    // moments (chat opened, brain switched, key added) — never on a timer.
    virtual AiBrainReply *verifyCredentialConnection(const AiCredential &credential,
                                                     QObject *replyParent) = 0;
};
