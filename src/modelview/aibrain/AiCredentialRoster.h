#pragma once

#include <QList>
#include <QObject>

#include "AiCredential.h"

// AiCredentialRoster
//
// The array of registered keys/providers, side by side (Hermes-style), that
// AIBrain draws from. Owns persistence of the roster and nothing else.
//
// Storage decision (interim, 2026-08-21): keys persist via QSettings under
// the application's native settings location. This is deliberately behind
// this class's interface so the open decision — Windows Credential Manager
// vs. encrypted QSettings — swaps the storage without touching a single
// caller. Revisit before any public release.
class AiCredentialRoster : public QObject {
    Q_OBJECT
public:
    explicit AiCredentialRoster(QObject *parent = nullptr);

    // Loads the roster from settings. Called once from the composition root.
    void loadFromSettings();

    // The full roster, in registration order.
    QList<AiCredential> allCredentials() const;

    int credentialCount() const;

    AiCredential credentialAt(int rosterIndex) const;

    // Registers a credential at the end of the roster and persists.
    void addCredential(const AiCredential &credential);

    // Removes by roster position and persists. Out-of-range is ignored.
    void removeCredentialAt(int rosterIndex);

    // Removes every credential for one provider and persists — the Settings
    // "Delete key" action clears the whole slot so no hidden sibling key
    // surprises anyone afterward.
    void removeAllCredentialsFor(AiProviderKind providerKind);

    // Flips a credential's enabled state and persists. Out-of-range ignored.
    void setCredentialEnabled(int rosterIndex, bool isEnabled);

    // The first enabled credential for the given provider — what AIBrain
    // uses to route a request. found is set accordingly.
    AiCredential firstEnabledCredentialFor(AiProviderKind providerKind, bool &found) const;

    // True when at least one enabled credential exists at all.
    bool hasAnyEnabledCredential() const;

signals:
    // The roster changed in any way (add, remove, enable/disable).
    // AIBrain and the Settings viewmodel both listen.
    void rosterChanged();

private:
    void saveToSettings() const;

    QList<AiCredential> registeredCredentials;
};
