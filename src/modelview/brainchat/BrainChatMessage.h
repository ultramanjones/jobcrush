#pragma once

#include <QString>

// BrainChatMessage
//
// One entry in the Brain Chat transcript. Pure data. A message can still be
// streaming (its text grows as fragments arrive) — the view renders growing
// text live, which IS the progress indicator (no-spinner law).
struct BrainChatMessage {
    enum class Author {
        Human,          // the user typed it
        Brain,          // AIBrain's response
        SystemNotice    // the app itself: errors, "no brain configured", etc.
    };

    Author author = Author::Human;
    QString messageText;
    bool isStillStreaming = false;
};
