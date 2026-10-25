#pragma once

#include <QList>
#include <QString>

// AiBrainLevel
//
// How much brain to use, separate from which brain. Every AI vendor sells a
// small fast model, a middle one, and a big one, and each vendor names them
// differently. A model's name, like "gpt-5.6-terra", means nothing to
// somebody who only wants a cover letter that sounds like them. So the user
// picks one of three plain levels, and each provider turns the level into its
// own model name. The real model name is still shown, small, for anyone who
// wants it.
//
// Stored in settings as plain words, the same as the provider kind.
enum class AiBrainLevel {
    Quick,      // the fastest and cheapest model
    Balanced,   // the usual choice
    Best        // the strongest model, and the most expensive
};

// All three levels, smallest first, in the order the picker shows them.
inline QList<AiBrainLevel> everyAiBrainLevel()
{
    return { AiBrainLevel::Quick, AiBrainLevel::Balanced, AiBrainLevel::Best };
}

// The name shown on screen, which is also the text stored in settings.
inline QString aiBrainLevelName(AiBrainLevel brainLevel)
{
    switch (brainLevel) {
    case AiBrainLevel::Quick:    return QStringLiteral("Quick");
    case AiBrainLevel::Balanced: return QStringLiteral("Balanced");
    case AiBrainLevel::Best:     return QStringLiteral("Best");
    }
    return QStringLiteral("Balanced");
}

// Reads a stored or shown name back. Anything unknown is Balanced, the
// usual choice.
inline AiBrainLevel aiBrainLevelFromName(const QString &levelName)
{
    if (levelName.compare(QStringLiteral("Quick"), Qt::CaseInsensitive) == 0) {
        return AiBrainLevel::Quick;
    }
    if (levelName.compare(QStringLiteral("Best"), Qt::CaseInsensitive) == 0) {
        return AiBrainLevel::Best;
    }
    return AiBrainLevel::Balanced;
}

// One plain sentence about what the level is for.
inline QString aiBrainLevelExplanation(AiBrainLevel brainLevel)
{
    switch (brainLevel) {
    case AiBrainLevel::Quick:
        return QStringLiteral("The fastest and cheapest. Fine for reading postings and "
                              "short answers.");
    case AiBrainLevel::Balanced:
        return QStringLiteral("The usual choice. Good writing at a fair price.");
    case AiBrainLevel::Best:
        return QStringLiteral("The strongest writing and reasoning. Costs the most per "
                              "question.");
    }
    return QString();
}
