#pragma once

#include <QString>

// PostingSource
//
// One place a job can be read or applied to. Pure data.
//
// A job is not in one place. The same opening is often on the employer's own
// board, on an aggregator that carried it, and behind whatever link the user
// happened to be given. Job Crush used to keep one of those and throw the rest
// away, which meant the app decided for the user which route they would take.
//
// It has no business deciding that. One person wants the employer's own form.
// Another already has an account on the aggregator and would rather use it. A
// third is checking whether the job is still open and the second link answers
// when the first has gone quiet. So every route that is found is kept, and the
// user picks.
struct PostingSource {
    qint64 jobPostingId = 0;   // which job this is a route to

    // The board or site, stored the same way everywhere else stores it:
    // "ashby", "lever", "greenhouse", "remotive". Empty is not allowed — a
    // route nobody can name is not a route anyone can choose.
    QString boardName;

    // That board's own id for the job, when it gave one. Empty is fine.
    QString externalId;

    // The link to open. This is the whole point of the record.
    QString postingUrl;
};
