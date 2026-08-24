#include "StagedDocumentRepository.h"

#include <algorithm>

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// An unassigned QString is null, not empty. Binding null to a NOT NULL column
// makes SQLite reject the row. Every repository needs this guard. This bug has
// happened three times on this project.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QString::fromLatin1("") : possiblyNullText;
}

QString timestampOrEmpty(const QDateTime &timestamp)
{
    return timestamp.isValid() ? timestamp.toString(Qt::ISODate) : QString::fromLatin1("");
}

StagedDocument stagedDocumentFromQueryRow(const QSqlQuery &row)
{
    StagedDocument stagedDocument;
    stagedDocument.stagedDocumentId =
        row.value(QStringLiteral("stagedDocumentId")).toLongLong();
    stagedDocument.jobApplicationId =
        row.value(QStringLiteral("jobApplicationId")).toLongLong();
    stagedDocument.documentKind = stagedDocumentKindFromStorageText(
        row.value(QStringLiteral("documentKind")).toString());
    stagedDocument.titleText    = row.value(QStringLiteral("titleText")).toString();
    stagedDocument.markdownText = row.value(QStringLiteral("markdownText")).toString();
    stagedDocument.wasWrittenByBrain =
        row.value(QStringLiteral("wasWrittenByBrain")).toInt() != 0;
    stagedDocument.wasEditedByUser =
        row.value(QStringLiteral("wasEditedByUser")).toInt() != 0;
    stagedDocument.isApprovedByUser =
        row.value(QStringLiteral("isApprovedByUser")).toInt() != 0;
    stagedDocument.createdTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("createdTimestamp")).toString(), Qt::ISODate);
    stagedDocument.lastEditedTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("lastEditedTimestamp")).toString(), Qt::ISODate);
    return stagedDocument;
}

// Packet order is defined in StagedDocument.h and SQL cannot call it. Sorting
// here keeps one definition of "letter first" instead of a copy in ORDER BY
// that could drift out of sync.
void sortIntoPacketOrder(QList<StagedDocument> &packet)
{
    std::stable_sort(packet.begin(), packet.end(),
                     [](const StagedDocument &left, const StagedDocument &right) {
                         return packetOrderOf(left.documentKind)
                              < packetOrderOf(right.documentKind);
                     });
}

} // namespace

StagedDocumentRepository::StagedDocumentRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool StagedDocumentRepository::insertStagedDocument(StagedDocument &stagedDocument)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO stagedDocument "
        "  (jobApplicationId, documentKind, titleText, markdownText,"
        "   wasWrittenByBrain, wasEditedByUser, isApprovedByUser,"
        "   createdTimestamp, lastEditedTimestamp) "
        "VALUES "
        "  (:jobApplicationId, :documentKind, :titleText, :markdownText,"
        "   :wasWrittenByBrain, :wasEditedByUser, :isApprovedByUser,"
        "   :createdTimestamp, :lastEditedTimestamp)"));
    insertQuery.bindValue(QStringLiteral(":jobApplicationId"), stagedDocument.jobApplicationId);
    insertQuery.bindValue(QStringLiteral(":documentKind"),
                          stagedDocumentKindToStorageText(stagedDocument.documentKind));
    insertQuery.bindValue(QStringLiteral(":titleText"), textOrEmpty(stagedDocument.titleText));
    insertQuery.bindValue(QStringLiteral(":markdownText"), textOrEmpty(stagedDocument.markdownText));
    insertQuery.bindValue(QStringLiteral(":wasWrittenByBrain"),
                          stagedDocument.wasWrittenByBrain ? 1 : 0);
    insertQuery.bindValue(QStringLiteral(":wasEditedByUser"),
                          stagedDocument.wasEditedByUser ? 1 : 0);
    insertQuery.bindValue(QStringLiteral(":isApprovedByUser"),
                          stagedDocument.isApprovedByUser ? 1 : 0);
    insertQuery.bindValue(QStringLiteral(":createdTimestamp"),
                          timestampOrEmpty(stagedDocument.createdTimestamp));
    insertQuery.bindValue(QStringLiteral(":lastEditedTimestamp"),
                          timestampOrEmpty(stagedDocument.lastEditedTimestamp));

    if (!insertQuery.exec()) {
        lastErrorDescription = insertQuery.lastError().text();
        return false;
    }
    stagedDocument.stagedDocumentId = insertQuery.lastInsertId().toLongLong();
    return true;
}

bool StagedDocumentRepository::updateStagedDocument(const StagedDocument &stagedDocument)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE stagedDocument SET"
        "  documentKind        = :documentKind,"
        "  titleText           = :titleText,"
        "  markdownText        = :markdownText,"
        "  wasWrittenByBrain   = :wasWrittenByBrain,"
        "  wasEditedByUser     = :wasEditedByUser,"
        "  isApprovedByUser    = :isApprovedByUser,"
        "  lastEditedTimestamp = :lastEditedTimestamp "
        "WHERE stagedDocumentId = :stagedDocumentId"));
    updateQuery.bindValue(QStringLiteral(":documentKind"),
                          stagedDocumentKindToStorageText(stagedDocument.documentKind));
    updateQuery.bindValue(QStringLiteral(":titleText"), textOrEmpty(stagedDocument.titleText));
    updateQuery.bindValue(QStringLiteral(":markdownText"), textOrEmpty(stagedDocument.markdownText));
    updateQuery.bindValue(QStringLiteral(":wasWrittenByBrain"),
                          stagedDocument.wasWrittenByBrain ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":wasEditedByUser"),
                          stagedDocument.wasEditedByUser ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":isApprovedByUser"),
                          stagedDocument.isApprovedByUser ? 1 : 0);
    updateQuery.bindValue(QStringLiteral(":lastEditedTimestamp"),
                          timestampOrEmpty(stagedDocument.lastEditedTimestamp));
    updateQuery.bindValue(QStringLiteral(":stagedDocumentId"), stagedDocument.stagedDocumentId);

    if (!updateQuery.exec()) {
        lastErrorDescription = updateQuery.lastError().text();
        return false;
    }
    return true;
}

bool StagedDocumentRepository::removeStagedDocument(qint64 stagedDocumentId)
{
    QSqlQuery deleteQuery(jobCrushDatabase.connection());
    deleteQuery.prepare(QStringLiteral(
        "DELETE FROM stagedDocument WHERE stagedDocumentId = :stagedDocumentId"));
    deleteQuery.bindValue(QStringLiteral(":stagedDocumentId"), stagedDocumentId);
    if (!deleteQuery.exec()) {
        lastErrorDescription = deleteQuery.lastError().text();
        return false;
    }
    return true;
}

QList<StagedDocument> StagedDocumentRepository::loadPacketForApplication(qint64 jobApplicationId)
{
    QList<StagedDocument> packet;
    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM stagedDocument WHERE jobApplicationId = :jobApplicationId "
        "ORDER BY stagedDocumentId ASC"));
    selectQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    if (!selectQuery.exec()) {
        lastErrorDescription = selectQuery.lastError().text();
        return packet;
    }
    while (selectQuery.next()) {
        packet.append(stagedDocumentFromQueryRow(selectQuery));
    }
    sortIntoPacketOrder(packet);
    return packet;
}

QList<StagedDocument> StagedDocumentRepository::loadEveryStagedDocument()
{
    QList<StagedDocument> everyDocument;
    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.exec(QStringLiteral(
        "SELECT * FROM stagedDocument ORDER BY jobApplicationId ASC, stagedDocumentId ASC"));
    while (selectQuery.next()) {
        everyDocument.append(stagedDocumentFromQueryRow(selectQuery));
    }
    return everyDocument;
}

int StagedDocumentRepository::countForApplication(qint64 jobApplicationId)
{
    QSqlQuery countQuery(jobCrushDatabase.connection());
    countQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM stagedDocument WHERE jobApplicationId = :jobApplicationId"));
    countQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    if (!countQuery.exec() || !countQuery.next()) {
        lastErrorDescription = countQuery.lastError().text();
        return 0;
    }
    return countQuery.value(0).toInt();
}

int StagedDocumentRepository::approvedCountForApplication(qint64 jobApplicationId)
{
    QSqlQuery countQuery(jobCrushDatabase.connection());
    countQuery.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM stagedDocument "
        "WHERE jobApplicationId = :jobApplicationId AND isApprovedByUser = 1"));
    countQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    if (!countQuery.exec() || !countQuery.next()) {
        lastErrorDescription = countQuery.lastError().text();
        return 0;
    }
    return countQuery.value(0).toInt();
}

bool StagedDocumentRepository::replaceGeneratedDocument(StagedDocument &stagedDocument,
                                                        bool &wasRefusedBecauseUserEdited)
{
    wasRefusedBecauseUserEdited = false;

    QSqlQuery existingQuery(jobCrushDatabase.connection());
    existingQuery.prepare(QStringLiteral(
        "SELECT * FROM stagedDocument "
        "WHERE jobApplicationId = :jobApplicationId AND documentKind = :documentKind "
        "ORDER BY stagedDocumentId ASC LIMIT 1"));
    existingQuery.bindValue(QStringLiteral(":jobApplicationId"), stagedDocument.jobApplicationId);
    existingQuery.bindValue(QStringLiteral(":documentKind"),
                            stagedDocumentKindToStorageText(stagedDocument.documentKind));
    if (!existingQuery.exec()) {
        lastErrorDescription = existingQuery.lastError().text();
        return false;
    }

    if (!existingQuery.next()) {
        return insertStagedDocument(stagedDocument);
    }

    const StagedDocument existingDocument = stagedDocumentFromQueryRow(existingQuery);
    if (existingDocument.wasEditedByUser) {
        // The user's own writing is kept. The caller tells the user this
        // happened, so the button does not look broken.
        wasRefusedBecauseUserEdited = true;
        stagedDocument.stagedDocumentId = existingDocument.stagedDocumentId;
        return true;
    }

    stagedDocument.stagedDocumentId = existingDocument.stagedDocumentId;
    stagedDocument.createdTimestamp = existingDocument.createdTimestamp;
    return updateStagedDocument(stagedDocument);
}

bool StagedDocumentRepository::removePacketForApplication(qint64 jobApplicationId)
{
    QSqlQuery deleteQuery(jobCrushDatabase.connection());
    deleteQuery.prepare(QStringLiteral(
        "DELETE FROM stagedDocument WHERE jobApplicationId = :jobApplicationId"));
    deleteQuery.bindValue(QStringLiteral(":jobApplicationId"), jobApplicationId);
    if (!deleteQuery.exec()) {
        lastErrorDescription = deleteQuery.lastError().text();
        return false;
    }
    return true;
}

QString StagedDocumentRepository::lastErrorText() const
{
    return lastErrorDescription;
}
