#include "AiBrain.h"

#include <QLocale>
#include <QSettings>

#include "AiBrainReply.h"
#include "AiBrainSoul.h"
#include "AiCredentialRoster.h"
#include "AnthropicApiProvider.h"
#include "GeminiApiProvider.h"
#include "OpenRouterApiProvider.h"

namespace {

// Routing order. This is NOT the selector — the user's explicit choice always
// wins. It only decides which brain Job Crush adopts on behalf of someone who
// has registered a key and never touched the selection checkboxes, so that
// one key is all it takes to get talking (the easiest-app-ever law).
const AiProviderKind providerRoutingOrder[] = {
    AiProviderKind::Anthropic,
    AiProviderKind::OpenRouter,
    AiProviderKind::OpenAi,
    AiProviderKind::Gemini,
    AiProviderKind::Ollama,
};

// Where the brain choice lives. Alongside the credential roster and the app
// preferences: app-wide QSettings, no accounts, set it and forget it.
const QString selectedProviderSettingsKey =
    QStringLiteral("aiBrain/selectedProviderKind");

// The stored value meaning "the user deliberately chose no brain at all",
// which is a legitimate state and must not be mistaken for "never chose".
const QString deliberatelyNoBrainSelectionText = QStringLiteral("none");

// "an Anthropic key" but "a Gemini key" — the article follows the vendor
// name, because copy that reads wrong makes an app feel careless.
QString indefiniteArticleFor(const QString &vendorName)
{
    if (vendorName.isEmpty()) {
        return QStringLiteral("a");
    }
    const QString vowelLetters = QStringLiteral("AEIOUaeiou");
    return vowelLetters.contains(vendorName.at(0))
        ? QStringLiteral("an")
        : QStringLiteral("a");
}

// The vendor's name, available even for providers whose client has not been
// written yet — the Settings page still has to be able to talk ABOUT them.
QString providerDisplayNameFor(AiProviderKind providerKind)
{
    switch (providerKind) {
    case AiProviderKind::Anthropic:  return QStringLiteral("Anthropic");
    case AiProviderKind::OpenRouter: return QStringLiteral("OpenRouter");
    case AiProviderKind::OpenAi:     return QStringLiteral("OpenAI");
    case AiProviderKind::Gemini:     return QStringLiteral("Gemini");
    case AiProviderKind::Ollama:     return QStringLiteral("Ollama");
    }
    return QString();
}

// The floor under every failure message. A provider that had nothing
// specific to say still does not get to leave the user with a dead end.
QString defaultNextStepFor(const QString &vendorName)
{
    return QStringLiteral(
        "Open the %1 tab above, delete the key and paste in a fresh copy from "
        "%1's own key page. If that changes nothing, check that you are online "
        "— Job Crush has to reach %1 over the internet to use it. Everything "
        "else in Job Crush keeps working in the meantime; only the AI parts "
        "need a brain.").arg(vendorName);
}

} // namespace

AiBrain::AiBrain(AiCredentialRoster &credentialRoster,
                 AiBrainSoul &soul,
                 QObject *parent)
    : QObject(parent)
    , registeredCredentialRoster(credentialRoster)
    , brainSoul(soul)
    , anthropicProvider(std::make_unique<AnthropicApiProvider>())
    , openRouterProvider(std::make_unique<OpenRouterApiProvider>())
    , geminiProvider(std::make_unique<GeminiApiProvider>())
{
    // Any roster edit in Settings changes what "connected and active" means,
    // instantly: a deleted key can unseat the running brain, and a freshly
    // pasted key is exactly the key moment a verification check exists for.
    connect(&registeredCredentialRoster, &AiCredentialRoster::rosterChanged,
            this, [this]() {
        resetConnectionStateAfterChange();
        emit selectedProviderChanged();
        emit configurationChanged();
        // A key was pasted, toggled or deleted in Settings — that is a person
        // acting, so a refusal here has earned the right to speak up.
        checkConnectionBecauseUserAskedFor();
    });
}

AiBrain::~AiBrain() = default;

void AiBrain::loadFromSettings()
{
    QSettings settings;
    storedSelectionText = settings.value(selectedProviderSettingsKey).toString();

    resetConnectionStateAfterChange();
    emit selectedProviderChanged();
    emit configurationChanged();
}

void AiBrain::persistSelectionToSettings() const
{
    QSettings settings;
    settings.setValue(selectedProviderSettingsKey, storedSelectionText);
}

AiBrainProvider *AiBrain::providerFor(AiProviderKind providerKind) const
{
    switch (providerKind) {
    case AiProviderKind::Anthropic:
        return anthropicProvider.get();
    case AiProviderKind::OpenRouter:
        return openRouterProvider.get();
    case AiProviderKind::Gemini:
        return geminiProvider.get();
    case AiProviderKind::OpenAi:
    case AiProviderKind::Ollama:
        return nullptr; // clients not written yet
    }
    return nullptr;
}

bool AiBrain::providerIsSelectable(AiProviderKind providerKind) const
{
    if (providerFor(providerKind) == nullptr) {
        return false;
    }
    bool credentialFound = false;
    registeredCredentialRoster.firstEnabledCredentialFor(providerKind, credentialFound);
    // (When the Ollama client lands it will need this credential requirement
    //  relaxed — it runs on the user's own machine and has no key at all.)
    return credentialFound;
}

QString AiBrain::reasonProviderCannotBeSelected(AiProviderKind providerKind) const
{
    if (providerIsSelectable(providerKind)) {
        return QString();
    }

    const QString vendorName = providerDisplayNameFor(providerKind);

    if (providerFor(providerKind) == nullptr) {
        // Nothing the user did — Job Crush simply hasn't learned this one yet.
        return QStringLiteral("Job Crush doesn't speak %1 yet — that one is "
                              "still being built.").arg(vendorName);
    }
    return QStringLiteral("Add %1 %2 key above and this brain can be selected.")
        .arg(indefiniteArticleFor(vendorName), vendorName);
}

AiProviderKind AiBrain::effectivelySelectedProviderKind(bool &found) const
{
    // The user deliberately chose no brain. Respect it — no silent fallback.
    if (storedSelectionText == deliberatelyNoBrainSelectionText) {
        found = false;
        return AiProviderKind::Anthropic;
    }

    // An explicit choice rules, as long as it can still work. (A deleted key
    // does not erase the choice; it just means nothing is active right now.)
    if (!storedSelectionText.isEmpty()) {
        const AiProviderKind chosenKind =
            aiProviderKindFromStorageText(storedSelectionText);
        found = providerIsSelectable(chosenKind);
        return chosenKind;
    }

    // Never chosen: adopt the first brain that can actually work. Registering
    // a single key is enough to start talking — the checkbox then shows the
    // adopted brain, so nothing about it is a secret.
    for (AiProviderKind candidateKind : providerRoutingOrder) {
        if (providerIsSelectable(candidateKind)) {
            found = true;
            return candidateKind;
        }
    }
    found = false;
    return AiProviderKind::Anthropic;
}

bool AiBrain::isConfigured() const
{
    bool found = false;
    effectivelySelectedProviderKind(found);
    return found;
}

QString AiBrain::selectedProviderKindName() const
{
    bool found = false;
    const AiProviderKind selectedKind = effectivelySelectedProviderKind(found);
    if (!found) {
        return QString();
    }
    return aiProviderKindToStorageText(selectedKind);
}

QString AiBrain::selectedProviderDisplayName() const
{
    bool found = false;
    const AiProviderKind selectedKind = effectivelySelectedProviderKind(found);
    if (!found) {
        return QString();
    }
    return providerDisplayNameFor(selectedKind);
}

void AiBrain::selectProviderKind(AiProviderKind providerKind)
{
    // A brain that cannot work is never offered as a choice, so this is a
    // belt-and-braces guard rather than a path the UI can reach.
    if (!providerIsSelectable(providerKind)) {
        return;
    }

    storedSelectionText = aiProviderKindToStorageText(providerKind);
    persistSelectionToSettings();

    resetConnectionStateAfterChange();
    emit selectedProviderChanged();
    emit configurationChanged();

    // Switching brains is a key moment: find out right now whether the new
    // one actually answers. The user just ticked a box, so if the vendor
    // refuses, they hear about it rather than watching the tick vanish.
    checkConnectionBecauseUserAskedFor();
}

void AiBrain::clearSelectedProvider()
{
    storedSelectionText = deliberatelyNoBrainSelectionText;
    persistSelectionToSettings();

    resetConnectionStateAfterChange();
    emit selectedProviderChanged();
    emit configurationChanged();
}

AiBrainConnectionState AiBrain::connectionState() const
{
    return currentConnectionState;
}

QString AiBrain::connectionStatusText() const
{
    return currentConnectionStatusText;
}

QString AiBrain::lastVerifiedAtText() const
{
    if (!lastSuccessfulVerificationTime.isValid()) {
        return QString();
    }
    return QStringLiteral("checked at %1").arg(
        QLocale().toString(lastSuccessfulVerificationTime.time(), QLocale::ShortFormat));
}

QString AiBrain::currentCredentialFingerprint() const
{
    bool found = false;
    const AiProviderKind selectedKind = effectivelySelectedProviderKind(found);
    if (!found) {
        return QString();
    }
    bool credentialFound = false;
    const AiCredential selectedCredential =
        registeredCredentialRoster.firstEnabledCredentialFor(selectedKind, credentialFound);

    // Provider plus key: swapping either one invalidates a cached "connected"
    // result, which is the entire point of keeping a fingerprint at all.
    return aiProviderKindToStorageText(selectedKind)
           + QStringLiteral("\n") + selectedCredential.secretKey;
}

void AiBrain::setConnectionState(AiBrainConnectionState newState,
                                 const QString &statusText)
{
    if (currentConnectionState == newState && currentConnectionStatusText == statusText) {
        return;
    }
    currentConnectionState = newState;
    currentConnectionStatusText = statusText;
    emit connectionStateChanged();
}

void AiBrain::resetConnectionStateAfterChange()
{
    // Whatever was in flight is answering a question nobody is asking now.
    if (activeVerificationReply != nullptr) {
        activeVerificationReply->disconnect(this);
        activeVerificationReply->deleteLater();
        activeVerificationReply = nullptr;
    }
    verifiedCredentialFingerprint.clear();
    lastSuccessfulVerificationTime = QDateTime();

    if (!isConfigured()) {
        setConnectionState(AiBrainConnectionState::NoBrainSelected, QString());
        return;
    }
    setConnectionState(AiBrainConnectionState::NotYetChecked, QString());
}

void AiBrain::checkConnectionBecauseUserAskedFor()
{
    // A person is standing there waiting for an answer, so a cached "we
    // already know this failed" is not good enough — ask again, properly.
    if (currentConnectionState == AiBrainConnectionState::ConnectionFailed) {
        setConnectionState(AiBrainConnectionState::NotYetChecked, QString());
    }
    checkInFlightWasStartedByUser = true;
    checkConnectionNow();
    // Nothing went out (no brain, or a cached success answered instantly), so
    // there is no pending refusal for this flag to belong to.
    if (currentConnectionState != AiBrainConnectionState::Checking) {
        checkInFlightWasStartedByUser = false;
    }
}

void AiBrain::checkConnectionNow()
{
    bool found = false;
    const AiProviderKind selectedKind = effectivelySelectedProviderKind(found);
    if (!found) {
        setConnectionState(AiBrainConnectionState::NoBrainSelected, QString());
        return;
    }

    // Already asking. One question at a time.
    if (currentConnectionState == AiBrainConnectionState::Checking) {
        return;
    }

    // The cached answer still applies to this exact brain-and-key pair, so
    // there is nothing to ask. This is what keeps Job Crush off the vendor's
    // servers between key moments.
    const QString fingerprint = currentCredentialFingerprint();
    if (currentConnectionState == AiBrainConnectionState::ConnectedAndActive
            && verifiedCredentialFingerprint == fingerprint) {
        return;
    }

    bool credentialFound = false;
    const AiCredential selectedCredential =
        registeredCredentialRoster.firstEnabledCredentialFor(selectedKind, credentialFound);
    if (!credentialFound) {
        setConnectionState(AiBrainConnectionState::NoBrainSelected, QString());
        return;
    }

    const QString vendorName = providerDisplayNameFor(selectedKind);
    setConnectionState(AiBrainConnectionState::Checking,
                       QStringLiteral("Confirming the connection to %1…").arg(vendorName));

    activeVerificationReply =
        providerFor(selectedKind)->verifyCredentialConnection(selectedCredential, this);

    connect(activeVerificationReply, &AiBrainReply::finished, this,
            [this, fingerprint](const QString &) {
        verifiedCredentialFingerprint = fingerprint;
        lastSuccessfulVerificationTime = QDateTime::currentDateTime();
        checkInFlightWasStartedByUser = false;
        setConnectionState(AiBrainConnectionState::ConnectedAndActive, QString());
        activeVerificationReply->deleteLater();
        activeVerificationReply = nullptr;
    });

    connect(activeVerificationReply, &AiBrainReply::failed, this,
            [this, vendorName](const QString &humanReadableReason) {
        // Facts about the situation, never a verdict about the person.
        const QString whatToDoNext = activeVerificationReply->suggestedNextStep();
        const bool announceOutLoud = checkInFlightWasStartedByUser;
        checkInFlightWasStartedByUser = false;

        setConnectionState(AiBrainConnectionState::ConnectionFailed, humanReadableReason);
        activeVerificationReply->deleteLater();
        activeVerificationReply = nullptr;

        if (announceOutLoud) {
            emit connectionAttemptRefused(vendorName, humanReadableReason,
                                          whatToDoNext.isEmpty()
                                              ? defaultNextStepFor(vendorName)
                                              : whatToDoNext);
        }
    });
}

AiBrainReply *AiBrain::streamConversation(
    const QList<AiBrainConversationMessage> &conversation,
    QObject *replyParent)
{
    bool found = false;
    const AiProviderKind selectedKind = effectivelySelectedProviderKind(found);
    if (!found) {
        return nullptr;
    }

    bool credentialFound = false;
    const AiCredential selectedCredential =
        registeredCredentialRoster.firstEnabledCredentialFor(selectedKind, credentialFound);
    if (!credentialFound) {
        return nullptr;
    }

    return providerFor(selectedKind)->streamConversation(
        brainSoul.assembledSoulText(), conversation, selectedCredential, replyParent);
}
