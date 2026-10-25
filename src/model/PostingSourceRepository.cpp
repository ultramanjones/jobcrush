#include "PostingSourceRepository.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>

#include "JobCrushDatabase.h"

PostingSourceRepository::PostingSourceRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool PostingSourceRepository::recordPostingSourceIfNew(const PostingSource &postingSource)
{
    // A route with no job, no board or no link is not a route. Refusing it
    // here keeps rows out of the table that the card would have to skip.
    // It returns true because skipping such a route is not a failure.
    if (postingSource.jobPostingId == 0
            || postingSource.boardName.trimmed().isEmpty()
            || postingSource.postingUrl.trimmed().isEmpty()) {
        return true;
    }

    // Ask first rather than leaning on the unique index to reject the insert.
    // "Do I already have this route?" is a normal question, and a yes is not
    // an error.
    QSqlQuery existingRouteQuery(jobCrushDatabase.connection());
    existingRouteQuery.prepare(QStringLiteral(
        "SELECT postingSourceId FROM jobPostingSource "
        "WHERE jobPostingId = :jobPostingId AND boardName = :boardName"));
    existingRouteQuery.bindValue(QStringLiteral(":jobPostingId"),
                                 postingSource.jobPostingId);
    existingRouteQuery.bindValue(QStringLiteral(":boardName"),
                                 postingSource.boardName);
    if (!existingRouteQuery.exec()) {
        lastErrorDescription = existingRouteQuery.lastError().text();
        return false;
    }
    if (existingRouteQuery.next()) {
        return true; // already known
    }

    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO jobPostingSource "
        "  (jobPostingId, boardName, externalId, postingUrl) "
        "VALUES "
        "  (:jobPostingId, :boardName, :externalId, :postingUrl)"));
    insertQuery.bindValue(QStringLiteral(":jobPostingId"), postingSource.jobPostingId);
    insertQuery.bindValue(QStringLiteral(":boardName"),    postingSource.boardName);
    // A QString nobody assigned is null, and a null binds as SQL NULL, which a
    // NOT NULL column refuses. This bug once cost the job posting table a
    // whole board's worth of jobs.
    insertQuery.bindValue(QStringLiteral(":externalId"),
                          postingSource.externalId.isNull()
                              ? QString::fromLatin1("") : postingSource.externalId);
    insertQuery.bindValue(QStringLiteral(":postingUrl"),   postingSource.postingUrl);

    if (!insertQuery.exec()) {
        lastErrorDescription = insertQuery.lastError().text();
        return false;
    }
    return true;
}

QList<PostingSource> PostingSourceRepository::loadPostingSourcesFor(qint64 jobPostingId)
{
    QList<PostingSource> postingSources;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM jobPostingSource WHERE jobPostingId = :jobPostingId "
        "ORDER BY postingSourceId ASC"));
    selectQuery.bindValue(QStringLiteral(":jobPostingId"), jobPostingId);
    if (!selectQuery.exec()) {
        // The caller gets an empty list, the same as a job with no routes.
        qWarning().noquote() << QStringLiteral(
            "PostingSourceRepository::loadPostingSourcesFor: query failed for job "
            "posting %1 — %2").arg(jobPostingId).arg(selectQuery.lastError().text());
        return postingSources;
    }

    while (selectQuery.next()) {
        PostingSource postingSource;
        postingSource.jobPostingId =
            selectQuery.value(QStringLiteral("jobPostingId")).toLongLong();
        postingSource.boardName =
            selectQuery.value(QStringLiteral("boardName")).toString();
        postingSource.externalId =
            selectQuery.value(QStringLiteral("externalId")).toString();
        postingSource.postingUrl =
            selectQuery.value(QStringLiteral("postingUrl")).toString();
        postingSources.append(postingSource);
    }
    return postingSources;
}

QString PostingSourceRepository::lastErrorText() const
{
    return lastErrorDescription;
}
