#pragma once

#include <QString>

#include "AiBrainTask.h"

// TaskReplyReader
//
// Turns the AI's reply into something the app can use.
//
// Models do not always follow the output format they were given. Read the
// reply loosely rather than throw away work the user paid for because the
// number came back as "FIT: 72%" instead of "FIT: 72". Nothing here makes up
// a value it did not find.
//
// No state, so it stays header-only.
class TaskReplyReader {
public:
    TaskOutcome read(AiBrainTaskKind taskKind, const QString &replyText) const;

private:
    // The fit percentage, or -1 if the reply did not give one.
    static int fitScoreIn(const QString &replyText);

    // Removes the chat wrapper models add even when told not to: a leading
    // "Sure, here's ...", a code fence around the whole answer, or a trailing
    // "Let me know if you'd like changes!".
    static QString withoutTheChatter(const QString &replyText);
};
