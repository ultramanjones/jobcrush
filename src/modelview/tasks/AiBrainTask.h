#pragma once

#include <QString>

#include "../../model/JobPosting.h"

// AiBrainTaskKind
//
// The fixed jobs the AI is asked to do. They are fixed so the output has a
// known shape every time. An open-ended request gives a different answer shape
// on every run, and nothing downstream can rely on it.
//
// Every task writes a draft into staging. No task sends anything. No task
// changes the board. The same rule is in the prime directives, so it is
// stated in both places.
enum class AiBrainTaskKind {
    ParsePosting,       // the posting, read back in plain words
    ScoreFit,           // how well this person matches, and honestly why not
    DraftCoverLetter,   // the letter, in the user's own voice
    TailorResume,       // their resume, aimed at this job
    SuggestFollowUp     // what to say after sending, and when
};

inline QString aiBrainTaskDisplayName(AiBrainTaskKind taskKind)
{
    switch (taskKind) {
    case AiBrainTaskKind::ParsePosting:     return QStringLiteral("Reading the posting");
    case AiBrainTaskKind::ScoreFit:         return QStringLiteral("Scoring the fit");
    case AiBrainTaskKind::DraftCoverLetter: return QStringLiteral("Drafting the letter");
    case AiBrainTaskKind::TailorResume:     return QStringLiteral("Tailoring the resume");
    case AiBrainTaskKind::SuggestFollowUp:  return QStringLiteral("Working out the follow-up");
    }
    return QStringLiteral("Working");
}

// TaskBriefing
//
// Everything the AI is told about one task. StagingWorkbench gathers it,
// because it is the layer that knows where each piece is stored.
// TaskInstructionWriter only turns a briefing into instructions. It never
// looks anything up itself.
struct TaskBriefing {
    JobPosting posting;

    // The user's history as plain text: jobs, schooling, skills.
    QString careerHistoryText;

    // The full text of the user's documents, not a summary. A letter written
    // from a summary comes out vague.
    QString professionalDocumentsText;

    // The user's own notes on this job, if they wrote any. These outrank
    // everything else in the briefing, and the instructions say so.
    QString userNotesText;

    // Anything the user typed into the box beside the button, such as
    // "mention the night shifts".
    QString extraInstructionText;
};

// TaskOutcome
//
// The result, after the reply has been read.
struct TaskOutcome {
    bool succeeded = false;
    QString markdownText;          // the draft, in the internal working format
    int fitScorePercent = -1;      // ScoreFit only; -1 everywhere else
    QString reasonText;            // why it failed, in plain words
    QString whatToDoNextText;      // what the user can do about it
};
