#pragma once

#include <QList>
#include <QString>

#include "PostingSource.h"

class JobCrushDatabase;

// PostingSourceRepository
//
// The model-layer gateway for the routes to one job. Everything above speaks
// in PostingSource structs; only this class speaks SQL.
class PostingSourceRepository {
public:
    explicit PostingSourceRepository(JobCrushDatabase &database);

    // Records a route unless this job already has one on the same board.
    //
    // Called every time a job is stored and every time a link is resolved, so
    // it has to be safe to call with something already known. A duplicate is
    // an ordinary outcome, not a failure.
    bool recordPostingSourceIfNew(const PostingSource &postingSource);

    // Every route to one job, in the order they were recorded — which is the
    // order they were found, and therefore the order they are worth trying.
    QList<PostingSource> loadPostingSourcesFor(qint64 jobPostingId);

    // Why the last write or query failed.
    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
