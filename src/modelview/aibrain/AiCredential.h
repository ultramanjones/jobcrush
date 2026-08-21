#pragma once

#include <QString>

// AiProviderKind
//
// The AI vendors AIBrain knows how to speak to. The roster can hold
// credentials for any of them; whether a PROVIDER class exists yet for a kind
// is a separate question (Phase 3 ships Anthropic first — see AiBrain.cpp).
// Stored in settings as human-readable text, same philosophy as the database.
enum class AiProviderKind {
    Anthropic,
    OpenAi,
    Ollama
};

// Converts an AiProviderKind to the exact text stored in settings
// (and shown, capitalized properly elsewhere, in the UI).
inline QString aiProviderKindToStorageText(AiProviderKind providerKind)
{
    switch (providerKind) {
    case AiProviderKind::Anthropic: return QStringLiteral("anthropic");
    case AiProviderKind::OpenAi:    return QStringLiteral("openai");
    case AiProviderKind::Ollama:    return QStringLiteral("ollama");
    }
    return QStringLiteral("anthropic"); // unreachable, but the compiler deserves certainty
}

// Converts stored text back to an AiProviderKind.
// Unknown text falls back to Anthropic rather than crashing.
inline AiProviderKind aiProviderKindFromStorageText(const QString &storageText)
{
    if (storageText == QStringLiteral("openai")) return AiProviderKind::OpenAi;
    if (storageText == QStringLiteral("ollama")) return AiProviderKind::Ollama;
    return AiProviderKind::Anthropic;
}

// AiCredential
//
// One entry in the credential roster: a key for one provider, registered
// side by side with its siblings (Hermes-style array of keys). Pure data.
//
// Auth strategy note: an API key is the always-works baseline strategy.
// OAuth ("Sign in with ChatGPT" and friends) slots in beside it later as a
// second credential source feeding this same struct — verified 2026-08-21:
// OpenAI's program is a closed beta (six named partners, no open enrollment),
// so the OAuth strategy stays a designed-for seam, not built yet.
struct AiCredential {
    AiProviderKind providerKind = AiProviderKind::Anthropic;
    QString secretKey;                    // the API key itself
    QString displayLabel;                 // the user's name for it: "personal key", "work key"
    bool isEnabled = true;                // disabled keys stay in the roster but are never used
};
