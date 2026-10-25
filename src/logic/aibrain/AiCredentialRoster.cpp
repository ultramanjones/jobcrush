#include "AiCredentialRoster.h"

#include <QDebug>
#include <QSettings>

namespace {
// Settings layout (QSettings arrays count from 1 on disk):
//   aiCredentialRoster/size             — how many entries
//   aiCredentialRoster/1/providerKind   — "anthropic" | "openrouter" | "openai"
//                                         | "gemini" | "ollama"
//   aiCredentialRoster/1/secretKey
//   aiCredentialRoster/1/displayLabel
//   aiCredentialRoster/1/isEnabled
const QString settingsGroupName = QStringLiteral("aiCredentialRoster");
} // namespace

AiCredentialRoster::AiCredentialRoster(QObject *parent)
    : QObject(parent)
{
}

void AiCredentialRoster::loadFromSettings()
{
    registeredCredentials.clear();

    QSettings settings;
    const int storedCredentialCount =
        settings.beginReadArray(settingsGroupName);
    for (int rosterIndex = 0; rosterIndex < storedCredentialCount; ++rosterIndex) {
        settings.setArrayIndex(rosterIndex);

        AiCredential credential;
        credential.providerKind = aiProviderKindFromStorageText(
            settings.value(QStringLiteral("providerKind")).toString());
        credential.secretKey = settings.value(QStringLiteral("secretKey")).toString();
        credential.displayLabel = settings.value(QStringLiteral("displayLabel")).toString();
        credential.isEnabled = settings.value(QStringLiteral("isEnabled"), true).toBool();

        registeredCredentials.append(credential);
    }
    settings.endArray();

    emit rosterChanged();
}

void AiCredentialRoster::saveToSettings() const
{
    QSettings settings;
    // The whole array is rewritten each time. It holds a few keys at most, and
    // removing the old group first means a shorter list leaves no stale
    // entries behind.
    settings.remove(settingsGroupName);
    settings.beginWriteArray(settingsGroupName, registeredCredentials.count());
    for (int rosterIndex = 0; rosterIndex < registeredCredentials.count(); ++rosterIndex) {
        settings.setArrayIndex(rosterIndex);
        const AiCredential &credential = registeredCredentials.at(rosterIndex);
        settings.setValue(QStringLiteral("providerKind"),
                          aiProviderKindToStorageText(credential.providerKind));
        settings.setValue(QStringLiteral("secretKey"), credential.secretKey);
        settings.setValue(QStringLiteral("displayLabel"), credential.displayLabel);
        settings.setValue(QStringLiteral("isEnabled"), credential.isEnabled);
    }
    settings.endArray();
}

QList<AiCredential> AiCredentialRoster::allCredentials() const
{
    return registeredCredentials;
}

int AiCredentialRoster::credentialCount() const
{
    return registeredCredentials.count();
}

AiCredential AiCredentialRoster::credentialAt(int rosterIndex) const
{
    if (rosterIndex < 0 || rosterIndex >= registeredCredentials.count()) {
        return AiCredential();
    }
    return registeredCredentials.at(rosterIndex);
}

void AiCredentialRoster::addCredential(const AiCredential &credential)
{
    registeredCredentials.append(credential);
    saveToSettings();
    emit rosterChanged();
}

void AiCredentialRoster::removeCredentialAt(int rosterIndex)
{
    if (rosterIndex < 0 || rosterIndex >= registeredCredentials.count()) {
        qWarning().noquote() << QStringLiteral(
            "AiCredentialRoster::removeCredentialAt: nothing removed for index %1 — "
            "the roster holds %2 keys")
            .arg(rosterIndex).arg(registeredCredentials.count());
        return;
    }
    registeredCredentials.removeAt(rosterIndex);
    saveToSettings();
    emit rosterChanged();
}

void AiCredentialRoster::removeAllCredentialsFor(AiProviderKind providerKind)
{
    bool anythingRemoved = false;
    for (int rosterIndex = registeredCredentials.count() - 1; rosterIndex >= 0; --rosterIndex) {
        if (registeredCredentials.at(rosterIndex).providerKind == providerKind) {
            registeredCredentials.removeAt(rosterIndex);
            anythingRemoved = true;
        }
    }
    if (anythingRemoved) {
        saveToSettings();
        emit rosterChanged();
    }
}

void AiCredentialRoster::setCredentialEnabled(int rosterIndex, bool isEnabled)
{
    if (rosterIndex < 0 || rosterIndex >= registeredCredentials.count()) {
        qWarning().noquote() << QStringLiteral(
            "AiCredentialRoster::setCredentialEnabled: nothing changed for index %1 — "
            "the roster holds %2 keys")
            .arg(rosterIndex).arg(registeredCredentials.count());
        return;
    }
    registeredCredentials[rosterIndex].isEnabled = isEnabled;
    saveToSettings();
    emit rosterChanged();
}

AiCredential AiCredentialRoster::firstEnabledCredentialFor(
    AiProviderKind providerKind, bool &found) const
{
    for (const AiCredential &credential : registeredCredentials) {
        if (credential.isEnabled && credential.providerKind == providerKind) {
            found = true;
            return credential;
        }
    }
    found = false;
    return AiCredential();
}

bool AiCredentialRoster::hasAnyEnabledCredential() const
{
    for (const AiCredential &credential : registeredCredentials) {
        if (credential.isEnabled) {
            return true;
        }
    }
    return false;
}
