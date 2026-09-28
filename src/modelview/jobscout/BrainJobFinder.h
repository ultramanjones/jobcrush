#pragma once

#include <QObject>
#include <QString>

#include "JobLead.h"

class AiBrain;
class AiBrainReply;
class JobScoutReply;

// BrainJobFinder
//
// Asks the connected AI brain to go find a job on the web.
//
// This is the last resort in Manual Add. It runs only after Job Crush has
// already tried the employer boards and the pasted page and come up empty,
// and only when a brain is connected, because a search costs the user
// money. The brain is told the company, the title and the link the user
// gave, and asked to search the web for the employer's own posting and
// answer with a small block of JSON: found or not, the title, the company,
// the place, and the posting's address.
//
// What comes back is a LEAD, not a posting. The brain's answer is a claim
// about where the job is; Job Crush then goes and reads that page itself,
// so what gets saved is the employer's words and not the brain's summary.
//
// The reply finishes with one posting-shaped result when the brain found a
// link and none when it did not. It fails when the brain could not be
// reached or gave an answer that could not be read.
class BrainJobFinder : public QObject {
public:
    explicit BrainJobFinder(AiBrain &aiBrain, QObject *parent = nullptr);

    // True when a brain is connected and can be asked.
    bool aBrainIsAvailable() const;

    // "Anthropic", "Gemini" — for the status line. Empty when none.
    QString brainDisplayName() const;

    // Ownership: the reply is parented to replyParent. Callers should
    // deleteLater() it once finished or failed has fired.
    JobScoutReply *findTheJob(const JobLead &jobLead, QObject *replyParent);

    // Reads the brain's answer. Public so it can be tested without a brain.
    // Returns false when the answer says the job was not found, or could not
    // be read at all (whyNot says which).
    static bool readAnswer(const QString &replyText, JobLead &foundLead, QString &whyNot);

private:
    static QString instructionsFor(const JobLead &jobLead);

    AiBrain &connectedBrain;
};
