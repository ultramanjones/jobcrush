#pragma once

#include <QAbstractListModel>
#include <QString>

class AiCredentialRoster;

// AiCredentialRosterViewModel
//
// Serves the registered AI credentials to the Settings page as list rows,
// and forwards the page's edits straight down to the roster. Instant-apply
// by law: every action persists the moment it happens.
//
// Secret keys never travel up in full — rows expose a masked form
// ("sk-…k3F9") so the Settings page can identify a key without displaying it.
class AiCredentialRosterViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int credentialCount READ rowCountForProperty NOTIFY credentialCountChanged)

    // Bumped on EVERY roster change (including enable toggles, which don't
    // change the count). QML bindings that call the per-provider invokables
    // below reference this property so they re-evaluate on any change.
    Q_PROPERTY(int rosterRevision READ rosterRevision NOTIFY rosterRevisionChanged)

public:
    enum RosterRole {
        ProviderKindNameRole = Qt::UserRole + 1, // "anthropic" | "openai" | "ollama"
        DisplayLabelRole,
        MaskedKeyRole,
        IsEnabledRole
    };

    explicit AiCredentialRosterViewModel(AiCredentialRoster &credentialRoster,
                                         QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountForProperty() const;

    int rosterRevision() const;

    // --- Per-provider slot view (the Settings chips) -------------------
    // The chips treat each provider as a key slot: green when filled.

    Q_INVOKABLE bool providerHasKey(const QString &providerKindName) const;

    // The nickname of the provider's (first) key; empty when none.
    Q_INVOKABLE QString providerKeyNickname(const QString &providerKindName) const;

    // The "Delete key" action: clears the provider's slot entirely.
    Q_INVOKABLE void deleteProviderKey(const QString &providerKindName);

    // Opens the provider's own official get-a-key page in the browser —
    // the "where do I get a key?" helping hand. Setup must never require
    // googling (easiest-app-ever law).
    Q_INVOKABLE void openProviderKeyInstructions(const QString &providerKindName) const;

    // Usage/spend lives on the provider's own dashboard — the one accurate
    // ledger. False only for local providers (Ollama) with nothing to meter.
    Q_INVOKABLE bool providerHasUsagePage(const QString &providerKindName) const;
    Q_INVOKABLE void openProviderUsagePage(const QString &providerKindName) const;

    // Paste guardrail: keys wear recognizable prefixes (sk-ant-, sk-or-,
    // AIza...). If the candidate key looks like it belongs to a DIFFERENT
    // provider than the selected slot, returns a friendly warning; empty
    // string when all is well. Advisory only — never blocks the add.
    Q_INVOKABLE QString keyFormatWarning(const QString &providerKindName,
                                         const QString &candidateKey) const;

    // The Settings page's "add key" form lands here.
    Q_INVOKABLE void addCredential(const QString &providerKindName,
                                   const QString &secretKey,
                                   const QString &displayLabel);

    Q_INVOKABLE void removeCredentialAt(int rosterIndex);

    Q_INVOKABLE void setCredentialEnabled(int rosterIndex, bool isEnabled);

signals:
    void credentialCountChanged();
    void rosterRevisionChanged();

private:
    AiCredentialRoster &roster;
    int rosterChangeCounter = 0;
};
