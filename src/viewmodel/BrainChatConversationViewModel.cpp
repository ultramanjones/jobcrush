#include "BrainChatConversationViewModel.h"

#include "../modelview/aibrain/AiBrain.h"
#include "../modelview/brainchat/BrainChatSession.h"

namespace {
// The author enum translated for QML delegates.
QString authorNameForRole(BrainChatMessage::Author author)
{
    switch (author) {
    case BrainChatMessage::Author::Human:        return QStringLiteral("human");
    case BrainChatMessage::Author::Brain:        return QStringLiteral("brain");
    case BrainChatMessage::Author::SystemNotice: return QStringLiteral("notice");
    }
    return QStringLiteral("notice");
}
} // namespace

BrainChatConversationViewModel::BrainChatConversationViewModel(
    BrainChatSession &chatSession, AiBrain &aiBrain, QObject *parent)
    : QAbstractListModel(parent)
    , brainChatSession(chatSession)
    , brain(aiBrain)
{
    // Session events → list model events, one to one.
    connect(&brainChatSession, &BrainChatSession::messageAppended, this,
            [this](int messageIndex) {
        beginInsertRows(QModelIndex(), messageIndex, messageIndex);
        endInsertRows();
    });

    connect(&brainChatSession, &BrainChatSession::messageUpdated, this,
            [this](int messageIndex) {
        const QModelIndex changedIndex = index(messageIndex);
        emit dataChanged(changedIndex, changedIndex);
    });

    connect(&brainChatSession, &BrainChatSession::conversationCleared, this,
            [this]() {
        beginResetModel();
        endResetModel();
    });

    connect(&brainChatSession, &BrainChatSession::brainRespondingChanged,
            this, &BrainChatConversationViewModel::brainRespondingChanged);

    connect(&brain, &AiBrain::configurationChanged,
            this, &BrainChatConversationViewModel::brainConfigurationChanged);
}

int BrainChatConversationViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0; // flat list, no children
    }
    return brainChatSession.messageCount();
}

QVariant BrainChatConversationViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid()) {
        return QVariant();
    }

    const BrainChatMessage chatMessage = brainChatSession.messageAt(modelIndex.row());
    switch (role) {
    case AuthorNameRole:      return authorNameForRole(chatMessage.author);
    case MessageTextRole:     return chatMessage.messageText;
    case IsStillStreamingRole: return chatMessage.isStillStreaming;
    }
    return QVariant();
}

QHash<int, QByteArray> BrainChatConversationViewModel::roleNames() const
{
    return {
        { AuthorNameRole,       QByteArrayLiteral("authorName") },
        { MessageTextRole,      QByteArrayLiteral("messageText") },
        { IsStillStreamingRole, QByteArrayLiteral("isStillStreaming") },
    };
}

bool BrainChatConversationViewModel::brainIsResponding() const
{
    return brainChatSession.brainIsResponding();
}

bool BrainChatConversationViewModel::brainIsConfigured() const
{
    return brain.isConfigured();
}

QString BrainChatConversationViewModel::activeProviderName() const
{
    return brain.selectedProviderDisplayName();
}

void BrainChatConversationViewModel::sendHumanMessage(const QString &messageText)
{
    brainChatSession.sendHumanMessage(messageText);
}

void BrainChatConversationViewModel::clearConversation()
{
    brainChatSession.clearConversation();
}
