#pragma once

#include <QStringList>

// MoonlightHints
//
// One-line hints for the Moonlight chat page, each naming something Job Crush
// can do that a user may not have found. The page shows one at a time and
// moves to the next every so often.
//
// Every hint names a feature that exists today. A hint pointing at something
// that is not there is worse than no hint, so when a feature changes, its
// hint changes in the same commit.
namespace MoonlightHints {

inline QStringList everyHint()
{
    return {
        QStringLiteral("Paste a job email or alert into Add a job on Discoveries, and "
                       "Moonlight fills the form in for you."),
        QStringLiteral("Paste a job posting into this chat and ask how well it fits "
                       "you."),
        QStringLiteral("ProDocs can save any of your documents as a Word, PDF or plain "
                       "text copy. Nothing leaves your computer."),
        QStringLiteral("Every job on your board has its own folder. Open a card and "
                       "click \"open this job's folder\"."),
        QStringLiteral("AI Buddy Mode in Settings lets ChatGPT, Claude or Gemini keep "
                       "your board up to date for you."),
        QStringLiteral("Rest the mouse on a column title on Job Pipelines to see what "
                       "belongs in it."),
        QStringLiteral("\"Are these still open?\" on Job Pipelines checks whether your "
                       "jobs are still listed."),
        QStringLiteral("Watch a company in Settings by pasting a link to any of its "
                       "jobs. Job Crush reads their whole board every time you scout."),
        QStringLiteral("Type the day you applied on any card: 10/8, yesterday, or 3 "
                       "days ago all work."),
    };
}

} // namespace MoonlightHints
