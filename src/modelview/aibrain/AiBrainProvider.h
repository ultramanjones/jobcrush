#pragma once

#include <QList>
#include <QString>

#include "AiCredential.h"

class AiBrainReply;
class QObject;

// AiBrainConversationMessage
//
// One turn of a conversation as providers see it: who spoke, and what they
// said. Deliberately vendor-neutral — each provider translates this into its
// own wire format (Anthropic JSON today; OpenAI/Ollama JSON later).
struct AiBrainConversationMessage {
    enum class Author {
        Human,      // the user
        Brain       // a previous AIBrain response, sent back for context
    };

    Author author = Author::Human;
    QString messageText;
};

// AiBrainProvider
//
// The interface every concrete provider implements (Anthropic, OpenAI,
// Ollama — interchangeable, per the plan). A provider is a thin HTTPS client
// speaking one vendor's JSON API: small, ordinary networking code. It holds
// no conversation state and no credentials of its own — everything it needs
// arrives with the call.
class AiBrainProvider {
public:
    virtual ~AiBrainProvider() = default;

    // The vendor name for logs and the UI, e.g. "Anthropic".
    virtual QString providerDisplayName() const = 0;

    // Starts a streaming request and returns immediately. The returned reply
    // (parented to replyParent) delivers fragments, then finished or failed.
    //
    // soulText   — the agent's identity/prime-directives, sent as the system
    //              prompt (see AiBrainSoul).
    // conversation — the full turn history, oldest first.
    virtual AiBrainReply *streamConversation(const QString &soulText,
                                             const QList<AiBrainConversationMessage> &conversation,
                                             const AiCredential &credential,
                                             QObject *replyParent) = 0;

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
