#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "BrainChatMessage.h"

class AiBrain;
class AiBrainReply;

// BrainChatSession
//
// The conversation itself — a ModelView resident. Owns the transcript,
// composes the context sent to AIBrain, and folds streamed fragments back
// into the growing Brain message. Knows nothing about views; the
// BrainChatConversationViewModel translates these signals into list rows.
//
// One session = one running conversation (v1 keeps a single session for the
// app's lifetime; saved chat history is a later decision).
class BrainChatSession : public QObject {
    Q_OBJECT
public:
    explicit BrainChatSession(AiBrain &aiBrain, QObject *parent = nullptr);

    int messageCount() const;
    BrainChatMessage messageAt(int messageIndex) const;

    // True while a Brain response is streaming in — the composer disables
    // its send button off this (one question at a time).
    bool brainIsResponding() const;

    // Appends the user's message and starts the streaming request.
    // Ignored (with a system notice) when no brain is configured.
    void sendHumanMessage(const QString &messageText);

    // Wipes the transcript for a fresh conversation.
    void clearConversation();

signals:
    // A message was appended at messageIndex.
    void messageAppended(int messageIndex);

    // The message at messageIndex changed (streaming text grew, or its
    // streaming flag flipped off).
    void messageUpdated(int messageIndex);

    // The whole transcript was reset.
    void conversationCleared();

    // brainIsResponding() changed.
    void brainRespondingChanged();

private:
    void appendMessage(const BrainChatMessage &message);
    void appendSystemNotice(const QString &noticeText);

    // The transcript translated into what providers expect: human and brain
    // turns only, system notices and the in-flight message excluded.
    QList<struct AiBrainConversationMessage> conversationForProvider() const;

    AiBrain &brain;
    QList<BrainChatMessage> transcriptMessages;
    AiBrainReply *activeBrainReply = nullptr; // owned via Qt parentage to this
};
