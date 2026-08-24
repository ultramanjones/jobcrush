#pragma once

#include <QString>

#include "AiBrainTask.h"

// TaskInstructionWriter
//
// Turns a briefing into the text sent to the AI.
//
// The app's four rules about the AI are enforced here, so they are written out
// in full:
//
//   1. Never invent. Every claim in a draft must come from the user's own
//      documents. A letter claiming a degree the user does not have will get
//      sent before they notice it is wrong.
//   2. Never send. Every output is a draft. The user reads it, fixes it, and
//      sends it. The soul files say this too. It is stated in both places on
//      purpose.
//   3. Use the user's voice, not the model's.
//   4. Return only the document. No preamble, no "here's your letter!". The
//      reply is stored as the document, so anything chatty ends up on the page
//      the employer reads.
//
// No state, so it stays a plain ModelView class.
class TaskInstructionWriter {
public:
    // The instruction text for one task. The soul files are not included here.
    // AiBrain sends those as the system prompt on every request. Adding them
    // here would mean two copies of the persona to keep in sync.
    QString instructionsFor(AiBrainTaskKind taskKind, const TaskBriefing &briefing) const;

private:
    // The parts of a briefing that every task needs.
    QString postingSectionOf(const TaskBriefing &briefing) const;
    QString personSectionOf(const TaskBriefing &briefing) const;
    QString housekeepingRules() const;
};
