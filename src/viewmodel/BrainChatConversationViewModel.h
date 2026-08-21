#pragma once

#include <QAbstractListModel>
#include <QString>

class AiBrain;
class BrainChatSession;

// BrainChatConversationViewModel
//
// Serves the Brain Chat transcript to the view as list rows, plus the small
// state the chat page binds to (is the brain responding? is one configured
// at all?). Translation and organization only — the conversation itself
// lives below, in BrainChatSession (ModelView).
//
// Named for the data it serves: the Brain Chat conversation. (Qt vocabulary
// note: QAbstractListModel is Qt's name; in our architecture this class is a
// viewmodel serving prepared rows.)
class BrainChatConversationViewModel : public QAbstractListModel {
    Q_OBJECT

    // True while an answer is streaming in; the composer disables send.
    Q_PROPERTY(bool brainIsResponding READ brainIsResponding NOTIFY brainRespondingChanged)

    // False in degraded mode (no key configured) — the page shows the path
    // to Settings instead of pretending a brain exists.
    Q_PROPERTY(bool brainIsConfigured READ brainIsConfigured NOTIFY brainConfigurationChanged)

    // "Anthropic" (etc.) for the chat header; empty when unconfigured.
    Q_PROPERTY(QString activeProviderName READ activeProviderName NOTIFY brainConfigurationChanged)

public:
    enum ConversationRole {
        AuthorNameRole = Qt::UserRole + 1,  // "human" | "brain" | "notice"
        MessageTextRole,
        IsStillStreamingRole
    };

    BrainChatConversationViewModel(BrainChatSession &chatSession,
                                   AiBrain &aiBrain,
                                   QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool brainIsResponding() const;
    bool brainIsConfigured() const;
    QString activeProviderName() const;

    // The composer's send button lands here.
    Q_INVOKABLE void sendHumanMessage(const QString &messageText);

    // The "new conversation" action.
    Q_INVOKABLE void clearConversation();

signals:
    void brainRespondingChanged();
    void brainConfigurationChanged();

private:
    BrainChatSession &brainChatSession;
    AiBrain &brain;
};
