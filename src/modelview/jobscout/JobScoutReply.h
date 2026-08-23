#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "../../model/JobPosting.h"

// JobScoutReply
//
// A live, in-flight sweep of ONE job site — the same idea as AiBrainReply,
// deliberately: you ask a source for jobs, it hands you one of these, and you
// connect to its signals.
//
// Ownership: parented to whoever asked. Callers should deleteLater() it once
// finished or failed has fired.
class JobScoutReply : public QObject {
    Q_OBJECT
public:
    explicit JobScoutReply(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    bool isFinished() const { return replyHasFinished; }

    // Called by the source when its results are parsed. Not for consumers.
    void markFinished(const QList<JobPosting> &foundJobPostings)
    {
        replyHasFinished = true;
        emit finished(foundJobPostings);
    }

    // Called by the source on any failure. Not for consumers.
    // The reason is written for human eyes and reaches the user as-is.
    void markFailed(const QString &humanReadableReason)
    {
        replyHasFinished = true;
        emit failed(humanReadableReason);
    }

signals:
    // The sweep of this one source succeeded. The postings are raw finds:
    // not yet deduplicated against what Job Crush already has, and not yet
    // scored — JobScout does both.
    void finished(const QList<JobPosting> &foundJobPostings);

    // Network trouble, a rejected request, or a response Job Crush could not
    // make sense of. One dead source never stops the others.
    void failed(const QString &humanReadableReason);

private:
    bool replyHasFinished = false;
};
