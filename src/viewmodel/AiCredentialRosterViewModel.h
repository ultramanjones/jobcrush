#pragma once

#include <QAbstractListModel>
#include <QString>

class AiCredentialRoster;

// AiCredentialRosterViewModel
//
// Serves the registered AI credentials to the Settings page as list rows,
// and forwards the page's edits straight down to the roster. Every action is
// saved the moment it happens. There is no Apply button.
//
// Secret keys never travel up in full. Rows carry a masked form
// ("sk-ant…k3F9") so the Settings page can name a key without showing it.
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

    // False for Ollama, which runs on the user's own computer and has no key.
    // Settings hides the key boxes for it and says what to do instead.
    Q_INVOKABLE bool providerNeedsKey(const QString &providerKindName) const;

    // The nickname of the provider's (first) key; empty when none.
    Q_INVOKABLE QString providerKeyNickname(const QString &providerKindName) const;

    // The "Delete key" action: clears the provider's slot entirely.
    Q_INVOKABLE void deleteProviderKey(const QString &providerKindName);

    // Opens the provider's own official get-a-key page in the browser, so the
    // user never has to search the web to find out where keys come from.
    Q_INVOKABLE void openProviderKeyInstructions(const QString &providerKindName) const;

    // Usage and spending live on the provider's own dashboard, which is the
    // only accurate record. False only for Ollama, which runs locally and has
    // nothing to bill.
    Q_INVOKABLE bool providerHasUsagePage(const QString &providerKindName) const;
    Q_INVOKABLE void openProviderUsagePage(const QString &providerKindName) const;

    // Paste check. Keys start with known prefixes (sk-ant-, sk-or-, AIza...).
    // If the pasted key looks like it belongs to a DIFFERENT provider than
    // the selected slot, this returns a warning. It returns an empty string
    // when all is well. It only warns. It never blocks the add.
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
