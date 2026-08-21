#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include <memory>

#include "AiBrainProvider.h"

class AiBrainReply;
class AiBrainSoul;
class AiCredentialRoster;

// AIBrain
//
// The front door of the AI subsystem — a ModelView resident. Everything
// above it (Brain Chat today; the task layer in Phase 5) asks AIBrain and
// never talks to a provider, a credential, or a network directly.
//
// Responsibilities, and nothing more:
//  - route a request to a provider that has an enabled credential
//    (Anthropic first in the routing order; OpenAI and Ollama providers
//    plug into providerFor() as they are written),
//  - carry the soul along as the system prompt,
//  - report plainly when no brain is configured (degraded mode: the rest
//    of Job Crush works fine without one).
//
// AiBrain borrows the roster and the soul — both are constructed and owned
// by the composition root and handed in by reference. It owns its providers.
class AiBrain : public QObject {
    Q_OBJECT
public:
    AiBrain(AiCredentialRoster &credentialRoster,
            AiBrainSoul &soul,
            QObject *parent = nullptr);
    ~AiBrain() override;

    // True when at least one enabled credential exists for a provider that
    // has an implementation. When false, callers should offer the user a
    // path to Settings instead of a chat composer.
    bool isConfigured() const;

    // Human-readable name of the provider a request would route to right
    // now, e.g. "Anthropic" — for the chat header. Empty when unconfigured.
    QString activeProviderDisplayName() const;

    // Starts a streaming conversation with the routed provider. The returned
    // reply (parented to replyParent) delivers fragments, then finished or
    // failed. Returns nullptr when no configured provider exists — callers
    // must check.
    AiBrainReply *streamConversation(const QList<AiBrainConversationMessage> &conversation,
                                     QObject *replyParent);

signals:
    // Configuration changed (a key added/removed/toggled in Settings).
    void configurationChanged();

private:
    // The provider implementation for a kind, or nullptr while that vendor's
    // client has not been written yet.
    AiBrainProvider *providerFor(AiProviderKind providerKind) const;

    // Picks the first provider kind (in routing order) that has both an
    // implementation and an enabled credential. found reports success.
    AiProviderKind routedProviderKind(bool &found) const;

    AiCredentialRoster &registeredCredentialRoster;
    AiBrainSoul &brainSoul;

    std::unique_ptr<AiBrainProvider> anthropicProvider;
};
