#pragma once

#include <QObject>
#include <QString>

// AiBrainReply
//
// A live, in-flight response from AIBrain — the same shape as Qt's own
// QNetworkReply idea: you ask AIBrain a question, it hands you one of these,
// and you connect to its signals. Text arrives in fragments as the provider
// streams it (streamed text IS the progress indicator — the no-spinner law
// is satisfied by nature here).
//
// Ownership: parented to whoever asked (Qt parent-child), and additionally
// self-owned in the sense that callers should deleteLater() it once finished
// or failed has fired.
class AiBrainReply : public QObject {
    Q_OBJECT
public:
    explicit AiBrainReply(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    // Everything streamed so far, accumulated for convenience.
    QString accumulatedText() const { return accumulatedResponseText; }

    bool isFinished() const { return replyHasFinished; }

    // Called by the provider as fragments arrive. Not for consumers.
    void appendStreamedFragment(const QString &textFragment)
    {
        accumulatedResponseText += textFragment;
        emit textFragmentReceived(textFragment);
    }

    // Called by the provider exactly once, on success. Not for consumers.
    void markFinished()
    {
        replyHasFinished = true;
        emit finished(accumulatedResponseText);
    }

    // Called by the provider exactly once, on any failure. Not for consumers.
    void markFailed(const QString &humanReadableReason)
    {
        replyHasFinished = true;
        emit failed(humanReadableReason);
    }

signals:
    // A new piece of response text just arrived from the provider.
    void textFragmentReceived(const QString &textFragment);

    // The full response arrived successfully.
    void finished(const QString &completeResponseText);

    // Something went wrong: network failure, bad key, provider error.
    // The reason is written for human eyes and lands in the chat as a notice.
    void failed(const QString &humanReadableReason);

private:
    QString accumulatedResponseText;
    bool replyHasFinished = false;
};
