#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

#include <memory>

#include "AiBrainProvider.h"

class AiBrainReply;
class AiBrainSoul;
class AiCredentialRoster;

// AiBrainConnectionState
//
// Everything Job Crush honestly knows about the selected brain right now.
// These are facts about the situation, never verdicts about the user — the
// UI copy built on them follows the same rule.
enum class AiBrainConnectionState {
    NoBrainSelected,     // nothing chosen, or the chosen brain cannot work yet
    NotYetChecked,       // chosen, but nothing verified since the last change
    Checking,            // a verification request is in flight right now
    ConnectedAndActive,  // the vendor accepted the key — this brain is live
    ConnectionFailed     // the vendor refused, or the network never answered
};

// AIBrain
//
// The front door of the AI subsystem — a ModelView resident. Everything
// above it (Brain Chat today; the task layer in Phase 5) asks AIBrain and
// never talks to a provider, a credential, or a network directly.
//
// Responsibilities, and nothing more:
//  - remember which brain the user SELECTED, and route every request to that
//    one (no silent fallback to a brain they did not choose),
//  - verify that the selected brain is genuinely reachable, at key moments
//    only — never on a timer,
//  - carry the soul along as the system prompt,
//  - report plainly when no brain is selected and active (degraded mode: the
//    rest of Job Crush works fine without one).
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

    // Loads the persisted brain choice. Called once from the composition
    // root, after the credential roster has loaded.
    void loadFromSettings();

    // --- Which brain answers ---------------------------------------------

    // True when a usable brain is selected — a client exists for it AND it
    // holds an enabled key. When false, callers should offer the user a path
    // to Settings instead of a chat composer.
    bool isConfigured() const;

    // The selected brain's storage name ("openrouter") and vendor name
    // ("OpenRouter"). Both empty when nothing usable is selected.
    QString selectedProviderKindName() const;
    QString selectedProviderDisplayName() const;

    // Whether a provider COULD be chosen right now: its client is written and
    // it holds an enabled key. The Settings checkbox is unavailable when this
    // is false — a brain that cannot work is never offered as a choice.
    bool providerIsSelectable(AiProviderKind providerKind) const;

    // Plain speech about why a provider cannot be chosen yet, for the line
    // beside its unavailable checkbox. Empty when it IS selectable.
    QString reasonProviderCannotBeSelected(AiProviderKind providerKind) const;

    // The user's explicit choice. Selecting immediately verifies the brain;
    // clearing leaves Job Crush with no active brain, which is a legitimate
    // state and not an error.
    void selectProviderKind(AiProviderKind providerKind);
    void clearSelectedProvider();

    // --- Is it actually connected? ----------------------------------------

    AiBrainConnectionState connectionState() const;

    // The whole truth in one sentence, written for a human: what is being
    // checked, or what to fix. Empty when there is nothing to say.
    QString connectionStatusText() const;

    // "checked at 2:15 PM" — the quiet receipt behind a green banner.
    // Empty until a check has succeeded.
    QString lastVerifiedAtText() const;

    // Runs a verification ping through the selected brain, at a KEY MOMENT
    // only: the chat screen opening, a brain being switched, a key being
    // added or deleted. A successful result is cached until one of those
    // moments changes something — Job Crush never polls a vendor in a loop.
    void checkConnectionNow();

    // --- Asking the brain something ---------------------------------------

    // Starts a streaming conversation with the selected brain. The returned
    // reply (parented to replyParent) delivers fragments, then finished or
    // failed. Returns nullptr when no usable brain is selected — callers
    // must check.
    AiBrainReply *streamConversation(const QList<AiBrainConversationMessage> &conversation,
                                     QObject *replyParent);

signals:
    // The selected brain changed, or the roster changed underneath it.
    void selectedProviderChanged();

    // connectionState()/connectionStatusText() changed.
    void connectionStateChanged();

    // Configuration changed (a key added/removed/toggled in Settings, or a
    // different brain selected).
    void configurationChanged();

private:
    // The provider implementation for a kind, or nullptr while that vendor's
    // client has not been written yet.
    AiBrainProvider *providerFor(AiProviderKind providerKind) const;

    // The brain that answers right now: the user's explicit choice when they
    // have made one, otherwise the first selectable brain in routing order
    // (default-until-first-choice — registering one key should not also
    // require ticking a box before anything works). found reports success.
    AiProviderKind effectivelySelectedProviderKind(bool &found) const;

    void persistSelectionToSettings() const;

    // Abandons any in-flight check and drops back to "nothing verified yet",
    // because something underneath the answer changed.
    void resetConnectionStateAfterChange();

    void setConnectionState(AiBrainConnectionState newState,
                            const QString &statusText);

    // Identifies WHICH brain-and-key pair a successful check applies to, so a
    // swapped key invalidates the cached "connected" result instead of
    // inheriting it.
    QString currentCredentialFingerprint() const;

    AiCredentialRoster &registeredCredentialRoster;
    AiBrainSoul &brainSoul;

    std::unique_ptr<AiBrainProvider> anthropicProvider;
    std::unique_ptr<AiBrainProvider> openRouterProvider;

    // The stored choice: empty means "never chosen" (adopt the default),
    // "none" means the user deliberately chose no brain, anything else is a
    // provider's storage name.
    QString storedSelectionText;

    AiBrainConnectionState currentConnectionState = AiBrainConnectionState::NoBrainSelected;
    QString currentConnectionStatusText;
    QString verifiedCredentialFingerprint;
    QDateTime lastSuccessfulVerificationTime;
    AiBrainReply *activeVerificationReply = nullptr; // owned via Qt parentage to this
};
