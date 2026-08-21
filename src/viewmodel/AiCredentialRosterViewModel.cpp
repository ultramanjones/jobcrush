#include "AiCredentialRosterViewModel.h"

#include <QDesktopServices>
#include <QUrl>

#include "../modelview/aibrain/AiCredentialRoster.h"

namespace {
// "sk-ant-api03-…k3F9" — enough to recognize, never enough to steal.
QString maskedFormOfSecretKey(const QString &secretKey)
{
    if (secretKey.length() <= 8) {
        return QStringLiteral("…");
    }
    return secretKey.left(6) + QStringLiteral("…") + secretKey.right(4);
}
} // namespace

AiCredentialRosterViewModel::AiCredentialRosterViewModel(
    AiCredentialRoster &credentialRoster, QObject *parent)
    : QAbstractListModel(parent)
    , roster(credentialRoster)
{
    // The roster is small and edits are rare — a full reset per change keeps
    // this translation layer honest and simple.
    connect(&roster, &AiCredentialRoster::rosterChanged, this, [this]() {
        beginResetModel();
        endResetModel();
        ++rosterChangeCounter;
        emit credentialCountChanged();
        emit rosterRevisionChanged();
    });
}

int AiCredentialRosterViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0; // flat list, no children
    }
    return roster.credentialCount();
}

QVariant AiCredentialRosterViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid()) {
        return QVariant();
    }

    const AiCredential credential = roster.credentialAt(modelIndex.row());
    switch (role) {
    case ProviderKindNameRole: return aiProviderKindToStorageText(credential.providerKind);
    case DisplayLabelRole:     return credential.displayLabel;
    case MaskedKeyRole:        return maskedFormOfSecretKey(credential.secretKey);
    case IsEnabledRole:        return credential.isEnabled;
    }
    return QVariant();
}

QHash<int, QByteArray> AiCredentialRosterViewModel::roleNames() const
{
    return {
        { ProviderKindNameRole, QByteArrayLiteral("providerKindName") },
        { DisplayLabelRole,     QByteArrayLiteral("displayLabel") },
        { MaskedKeyRole,        QByteArrayLiteral("maskedKey") },
        { IsEnabledRole,        QByteArrayLiteral("isEnabled") },
    };
}

int AiCredentialRosterViewModel::rowCountForProperty() const
{
    return roster.credentialCount();
}

int AiCredentialRosterViewModel::rosterRevision() const
{
    return rosterChangeCounter;
}

bool AiCredentialRosterViewModel::providerHasKey(const QString &providerKindName) const
{
    const AiProviderKind providerKind = aiProviderKindFromStorageText(providerKindName);
    for (const AiCredential &credential : roster.allCredentials()) {
        if (credential.providerKind == providerKind) {
            return true;
        }
    }
    return false;
}

QString AiCredentialRosterViewModel::providerKeyNickname(const QString &providerKindName) const
{
    const AiProviderKind providerKind = aiProviderKindFromStorageText(providerKindName);
    for (const AiCredential &credential : roster.allCredentials()) {
        if (credential.providerKind == providerKind) {
            return credential.displayLabel;
        }
    }
    return QString();
}

void AiCredentialRosterViewModel::deleteProviderKey(const QString &providerKindName)
{
    roster.removeAllCredentialsFor(aiProviderKindFromStorageText(providerKindName));
}

void AiCredentialRosterViewModel::openProviderKeyInstructions(
    const QString &providerKindName) const
{
    // Each provider's OFFICIAL page — never a third-party tutorial.
    // (Ollama has no keys; its page is the local install download.)
    QString instructionsUrl;
    switch (aiProviderKindFromStorageText(providerKindName)) {
    case AiProviderKind::Anthropic:
        instructionsUrl = QStringLiteral("https://console.anthropic.com/settings/keys");
        break;
    case AiProviderKind::OpenAi:
        instructionsUrl = QStringLiteral("https://platform.openai.com/api-keys");
        break;
    case AiProviderKind::Ollama:
        instructionsUrl = QStringLiteral("https://ollama.com/download");
        break;
    }
    QDesktopServices::openUrl(QUrl(instructionsUrl));
}

void AiCredentialRosterViewModel::addCredential(const QString &providerKindName,
                                                const QString &secretKey,
                                                const QString &displayLabel)
{
    const QString trimmedSecretKey = secretKey.trimmed();
    if (trimmedSecretKey.isEmpty()) {
        return; // nothing to register
    }

    AiCredential credential;
    credential.providerKind = aiProviderKindFromStorageText(providerKindName);
    credential.secretKey = trimmedSecretKey;
    credential.displayLabel = displayLabel.trimmed().isEmpty()
        ? QStringLiteral("unnamed key")
        : displayLabel.trimmed();
    credential.isEnabled = true;

    roster.addCredential(credential);
}

void AiCredentialRosterViewModel::removeCredentialAt(int rosterIndex)
{
    roster.removeCredentialAt(rosterIndex);
}

void AiCredentialRosterViewModel::setCredentialEnabled(int rosterIndex, bool isEnabled)
{
    roster.setCredentialEnabled(rosterIndex, isEnabled);
}
