#pragma once

#include <QString>

#include "../../model/JobPosting.h"

class AiBrain;
class JobScoutReply;
class QObject;

// PastedTextJobReader
//
// Reads a job out of text the user pasted: an email, a job alert, a
// recruiter message, or a posting copied off a page no feed can reach.
//
// It asks the connected AI brain to pull out the company, the title, the
// place, the pay, whether it is remote, the link, the posting date and the
// description, and to answer in a fixed JSON shape. Anything the text does
// not say comes back empty, never guessed: the brain is told so, and the form
// then marks those boxes as still needing the user.
//
// No web search. Reading text the user already has is cheap, and searching
// would turn a few cents into more. Finding the employer's own posting comes
// after, through the same hunt "Add a job" always runs.
class PastedTextJobReader {
public:
    explicit PastedTextJobReader(AiBrain &aiBrain);

    // The reply finishes with one posting holding whatever was found. It
    // fails when no brain is connected, the brain could not be reached, or
    // the answer could not be read.
    JobScoutReply *readJob(const QString &pastedText, QObject *replyParent);

    // Reads the brain's answer. Public so it can be tested without a brain.
    static bool readAnswer(const QString &replyText, JobPosting &postingRead,
                           QString &whyNot);

private:
    static QString instructionsFor(const QString &pastedText);

    AiBrain &connectedBrain;
};
