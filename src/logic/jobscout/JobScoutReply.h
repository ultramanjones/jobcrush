#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

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

    // Called by a source that hears from many companies, each time one
    // company answers. JobScout saves those jobs right away, one company at
    // a time, so the user sees them arrive instead of waiting for the end.
    // A source that calls this still calls markFinished with everything at
    // the end, and JobScout does not save those jobs a second time.
    // Not for consumers.
    void deliverSomeJobs(const QList<JobPosting> &someFoundJobPostings)
    {
        jobsWereDeliveredAsTheyArrived = true;
        emit someJobsFound(someFoundJobPostings);
    }
    bool deliveredJobsAsTheyArrived() const { return jobsWereDeliveredAsTheyArrived; }

    // Called by the source on any failure. Not for consumers.
    // The reason is written for human eyes and reaches the user as-is.
    //
    // sourceHadTrouble separates two failures that look the same and are not.
    // "There is no board by that name" is an ANSWER — the thing asked for
    // does not exist. "The connection timed out" is TROUBLE — the thing may
    // exist and we could not find out. Telling a user their employer is not on
    // Greenhouse when the truth is their wifi dropped sends them off fixing
    // the wrong problem.
    void markFailed(const QString &humanReadableReason, bool sourceHadTrouble = true)
    {
        replyHasFinished = true;
        emit failed(humanReadableReason, sourceHadTrouble);
    }

    // The employers whose WHOLE board was read in this sweep, as
    // "board/tenant" keys such as "greenhouse/acme". Only the companies-you-
    // follow source fills this in. A job from one of these employers that is
    // missing from the answer has been taken down, and JobScout marks it.
    // Set by the source before markFinished. Empty for every other source,
    // because a search site only ever shows a slice of a company's jobs.
    void setEmployersReadInFull(const QStringList &employerKeys)
    {
        employersReadInFullKeys = employerKeys;
    }
    QStringList employersReadInFull() const { return employersReadInFullKeys; }

    // True when a board reader stopped before the end of a company's board,
    // or only searched it. Then a job missing from the answer may not have
    // been read, so it must never be taken for gone.
    void setReadOnlyPartOfTheBoard(bool readOnlyPart) { onlyPartOfTheBoardWasRead = readOnlyPart; }
    bool readOnlyPartOfTheBoard() const { return onlyPartOfTheBoardWasRead; }

    // True when a board reader stopped at its page limit while the board or
    // the search still had more jobs to give. Then even a search's answer is
    // not complete, and a job missing from it is not known to be gone.
    void setStoppedAtThePageLimit(bool stopped) { stoppedAtPageLimit = stopped; }
    bool stoppedAtThePageLimit() const { return stoppedAtPageLimit; }

signals:
    // The sweep of this one source succeeded. The postings are raw finds:
    // not yet deduplicated against what Job Crush already has, and not yet
    // scored — JobScout does both.
    void finished(const QList<JobPosting> &foundJobPostings);

    // Some of the jobs, from one company, while the source is still working.
    // See deliverSomeJobs.
    void someJobsFound(const QList<JobPosting> &someFoundJobPostings);

    // Network trouble, a rejected request, or a response Job Crush could not
    // make sense of. One dead source never stops the others.
    //
    // Handlers that only care about the words may take one argument.
    void failed(const QString &humanReadableReason, bool sourceHadTrouble);

private:
    bool replyHasFinished = false;
    QStringList employersReadInFullKeys;
    bool onlyPartOfTheBoardWasRead = false;
    bool stoppedAtPageLimit = false;
    bool jobsWereDeliveredAsTheyArrived = false;
};
