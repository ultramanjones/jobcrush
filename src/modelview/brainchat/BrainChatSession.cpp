#include "BrainChatSession.h"

#include "../aibrain/AiBrain.h"
#include "../aibrain/AiBrainProvider.h"
#include "../aibrain/AiBrainReply.h"

BrainChatSession::BrainChatSession(AiBrain &aiBrain, QObject *parent)
    : QObject(parent)
    , brain(aiBrain)
{
}

int BrainChatSession::messageCount() const
{
    return transcriptMessages.count();
}

BrainChatMessage BrainChatSession::messageAt(int messageIndex) const
{
    if (messageIndex < 0 || messageIndex >= transcriptMessages.count()) {
        return BrainChatMessage();
    }
    return transcriptMessages.at(messageIndex);
}

bool BrainChatSession::brainIsResponding() const
{
    return activeBrainReply != nullptr;
}

void BrainChatSession::appendMessage(const BrainChatMessage &message)
{
    transcriptMessages.append(message);
    emit messageAppended(transcriptMessages.count() - 1);
}

void BrainChatSession::appendSystemNotice(const QString &noticeText)
{
    BrainChatMessage noticeMessage;
    noticeMessage.author = BrainChatMessage::Author::SystemNotice;
    noticeMessage.messageText = noticeText;
    appendMessage(noticeMessage);
}

QList<AiBrainConversationMessage> BrainChatSession::conversationForProvider() const
{
    // Providers get the human/brain turns only. System notices are app
    // furniture, and a still-streaming message is the answer being written —
    // neither belongs in the context we send.
    QList<AiBrainConversationMessage> providerConversation;
    for (const BrainChatMessage &transcriptMessage : transcriptMessages) {
        if (transcriptMessage.isStillStreaming) {
            continue;
        }
        switch (transcriptMessage.author) {
        case BrainChatMessage::Author::Human: {
            AiBrainConversationMessage providerMessage;
            providerMessage.author = AiBrainConversationMessage::Author::Human;
            providerMessage.messageText = transcriptMessage.messageText;
            providerConversation.append(providerMessage);
            break;
        }
        case BrainChatMessage::Author::Brain: {
            AiBrainConversationMessage providerMessage;
            providerMessage.author = AiBrainConversationMessage::Author::Brain;
            providerMessage.messageText = transcriptMessage.messageText;
            providerConversation.append(providerMessage);
            break;
        }
        case BrainChatMessage::Author::SystemNotice:
            break; // never sent to providers
        }
    }
    return providerConversation;
}

void BrainChatSession::sendHumanMessage(const QString &messageText)
{
    const QString trimmedMessageText = messageText.trimmed();
    if (trimmedMessageText.isEmpty() || brainIsResponding()) {
        return;
    }

    if (!brain.isConfigured()) {
        // Degraded mode: the message still lands in the transcript so the
        // user's typing is never eaten, followed by an honest explanation.
        BrainChatMessage humanMessage;
        humanMessage.author = BrainChatMessage::Author::Human;
        humanMessage.messageText = trimmedMessageText;
        appendMessage(humanMessage);
        appendSystemNotice(QStringLiteral(
            "No AI provider is configured yet. Add an API key in Settings and "
            "this conversation picks up right where it left off."));
        return;
    }

    BrainChatMessage humanMessage;
    humanMessage.author = BrainChatMessage::Author::Human;
    humanMessage.messageText = trimmedMessageText;
    appendMessage(humanMessage);

    // The Brain's answer starts life empty and grows as fragments arrive.
    BrainChatMessage brainMessage;
    brainMessage.author = BrainChatMessage::Author::Brain;
    brainMessage.isStillStreaming = true;
    appendMessage(brainMessage);
    const int brainMessageIndex = transcriptMessages.count() - 1;

    activeBrainReply = brain.streamConversation(conversationForProvider(), this);
    if (activeBrainReply == nullptr) {
        // Configuration vanished between the check above and the call —
        // possible if a key was deleted in Settings mid-keystroke.
        transcriptMessages[brainMessageIndex].author = BrainChatMessage::Author::SystemNotice;
        transcriptMessages[brainMessageIndex].messageText =
            QStringLiteral("The configured AI provider went away before the "
                           "request could start. Check Settings.");
        transcriptMessages[brainMessageIndex].isStillStreaming = false;
        emit messageUpdated(brainMessageIndex);
        return;
    }
    emit brainRespondingChanged();

    connect(activeBrainReply, &AiBrainReply::textFragmentReceived, this,
            [this, brainMessageIndex](const QString &textFragment) {
        transcriptMessages[brainMessageIndex].messageText += textFragment;
        emit messageUpdated(brainMessageIndex);
    });

    connect(activeBrainReply, &AiBrainReply::finished, this,
            [this, brainMessageIndex](const QString &) {
        transcriptMessages[brainMessageIndex].isStillStreaming = false;
        emit messageUpdated(brainMessageIndex);
        activeBrainReply->deleteLater();
        activeBrainReply = nullptr;
        emit brainRespondingChanged();
    });

    connect(activeBrainReply, &AiBrainReply::failed, this,
            [this, brainMessageIndex](const QString &humanReadableReason) {
        // The unanswered message becomes an honest notice — never a blank
        // bubble pretending something happened.
        transcriptMessages[brainMessageIndex].author = BrainChatMessage::Author::SystemNotice;
        transcriptMessages[brainMessageIndex].messageText =
            QStringLiteral("Moonlight could not answer: %1").arg(humanReadableReason);
        transcriptMessages[brainMessageIndex].isStillStreaming = false;
        emit messageUpdated(brainMessageIndex);
        activeBrainReply->deleteLater();
        activeBrainReply = nullptr;
        emit brainRespondingChanged();
    });
}

void BrainChatSession::clearConversation()
{
    // A response mid-stream keeps running server-side but its fragments have
    // nowhere to land; drop the reply object and start clean.
    if (activeBrainReply != nullptr) {
        activeBrainReply->deleteLater();
        activeBrainReply = nullptr;
        emit brainRespondingChanged();
    }
    transcriptMessages.clear();
    emit conversationCleared();
}
