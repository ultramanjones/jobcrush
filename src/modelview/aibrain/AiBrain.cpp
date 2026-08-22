#include "AiBrain.h"

#include "AiBrainReply.h"
#include "AiBrainSoul.h"
#include "AiCredentialRoster.h"
#include "AnthropicApiProvider.h"

namespace {
// Routing order: the first kind here with an implementation AND an enabled
// credential wins. Fallback-on-failure (skipping a rate-limited provider)
// arrives with the task layer; for now this is the whole routing policy.
const AiProviderKind providerRoutingOrder[] = {
    AiProviderKind::Anthropic,
    AiProviderKind::OpenAi,
    AiProviderKind::Gemini,
    AiProviderKind::Ollama,
};
} // namespace

AiBrain::AiBrain(AiCredentialRoster &credentialRoster,
                 AiBrainSoul &soul,
                 QObject *parent)
    : QObject(parent)
    , registeredCredentialRoster(credentialRoster)
    , brainSoul(soul)
    , anthropicProvider(std::make_unique<AnthropicApiProvider>())
{
    // Any roster edit in Settings changes what "configured" means, instantly.
    connect(&registeredCredentialRoster, &AiCredentialRoster::rosterChanged,
            this, &AiBrain::configurationChanged);
}

AiBrain::~AiBrain() = default;

AiBrainProvider *AiBrain::providerFor(AiProviderKind providerKind) const
{
    switch (providerKind) {
    case AiProviderKind::Anthropic:
        return anthropicProvider.get();
    case AiProviderKind::OpenAi:
    case AiProviderKind::Gemini:
    case AiProviderKind::Ollama:
        return nullptr; // clients not written yet — Phase 3 ships Anthropic first
    }
    return nullptr;
}

AiProviderKind AiBrain::routedProviderKind(bool &found) const
{
    for (AiProviderKind candidateKind : providerRoutingOrder) {
        if (providerFor(candidateKind) == nullptr) {
            continue;
        }
        bool credentialFound = false;
        registeredCredentialRoster.firstEnabledCredentialFor(candidateKind, credentialFound);
        if (credentialFound) {
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
    routedProviderKind(found);
    return found;
}

QString AiBrain::activeProviderDisplayName() const
{
    bool found = false;
    const AiProviderKind activeKind = routedProviderKind(found);
    if (!found) {
        return QString();
    }
    return providerFor(activeKind)->providerDisplayName();
}

AiBrainReply *AiBrain::streamConversation(
    const QList<AiBrainConversationMessage> &conversation,
    QObject *replyParent)
{
    bool found = false;
    const AiProviderKind activeKind = routedProviderKind(found);
    if (!found) {
        return nullptr;
    }

    bool credentialFound = false;
    const AiCredential activeCredential =
        registeredCredentialRoster.firstEnabledCredentialFor(activeKind, credentialFound);

    return providerFor(activeKind)->streamConversation(
        brainSoul.assembledSoulText(), conversation, activeCredential, replyParent);
}
