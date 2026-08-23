#include "ProfessionalDocumentRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "JobCrushDatabase.h"

namespace {

// An unassigned QString is null, not empty, and binding null to a NOT NULL
// column makes SQLite refuse the whole row. Same guard the job repository
// carries, and for the same hard-won reason.
QString textOrEmpty(const QString &possiblyNullText)
{
    return possiblyNullText.isNull() ? QString::fromLatin1("") : possiblyNullText;
}

ProfessionalDocument professionalDocumentFromQueryRow(const QSqlQuery &row)
{
    ProfessionalDocument professionalDocument;
    professionalDocument.professionalDocumentId =
        row.value(QStringLiteral("professionalDocumentId")).toLongLong();
    professionalDocument.documentKind = row.value(QStringLiteral("documentKind")).toString();
    professionalDocument.displayName = row.value(QStringLiteral("displayName")).toString();
    professionalDocument.originalFilePath =
        row.value(QStringLiteral("originalFilePath")).toString();
    professionalDocument.storedFilePath =
        row.value(QStringLiteral("storedFilePath")).toString();
    professionalDocument.extractedText = row.value(QStringLiteral("extractedText")).toString();
    professionalDocument.importedTimestamp = QDateTime::fromString(
        row.value(QStringLiteral("importedTimestamp")).toString(), Qt::ISODate);
    professionalDocument.fileSizeBytes =
        row.value(QStringLiteral("fileSizeBytes")).toLongLong();
    professionalDocument.textExtractionNote =
        row.value(QStringLiteral("textExtractionNote")).toString();
    professionalDocument.hasBeenReadForInsights =
        row.value(QStringLiteral("hasBeenReadForInsights")).toInt() != 0;
    return professionalDocument;
}

} // namespace

ProfessionalDocumentRepository::ProfessionalDocumentRepository(JobCrushDatabase &database)
    : jobCrushDatabase(database)
{
}

bool ProfessionalDocumentRepository::insertProfessionalDocument(
    ProfessionalDocument &professionalDocument)
{
    QSqlQuery insertQuery(jobCrushDatabase.connection());
    insertQuery.prepare(QStringLiteral(
        "INSERT INTO professionalDocument "
        "  (documentKind, displayName, originalFilePath, storedFilePath,"
        "   extractedText, importedTimestamp, fileSizeBytes, textExtractionNote) "
        "VALUES "
        "  (:documentKind, :displayName, :originalFilePath, :storedFilePath,"
        "   :extractedText, :importedTimestamp, :fileSizeBytes, :textExtractionNote)"));

    insertQuery.bindValue(QStringLiteral(":documentKind"),
                          textOrEmpty(professionalDocument.documentKind));
    insertQuery.bindValue(QStringLiteral(":displayName"),
                          textOrEmpty(professionalDocument.displayName));
    insertQuery.bindValue(QStringLiteral(":originalFilePath"),
                          textOrEmpty(professionalDocument.originalFilePath));
    insertQuery.bindValue(QStringLiteral(":storedFilePath"),
                          textOrEmpty(professionalDocument.storedFilePath));
    insertQuery.bindValue(QStringLiteral(":extractedText"),
                          textOrEmpty(professionalDocument.extractedText));
    insertQuery.bindValue(QStringLiteral(":importedTimestamp"),
                          textOrEmpty(professionalDocument.importedTimestamp.toString(Qt::ISODate)));
    insertQuery.bindValue(QStringLiteral(":fileSizeBytes"),
                          professionalDocument.fileSizeBytes);
    insertQuery.bindValue(QStringLiteral(":textExtractionNote"),
                          textOrEmpty(professionalDocument.textExtractionNote));

    if (!insertQuery.exec()) {
        lastErrorDescription = insertQuery.lastError().text();
        return false;
    }
    professionalDocument.professionalDocumentId = insertQuery.lastInsertId().toLongLong();
    return true;
}

QList<ProfessionalDocument> ProfessionalDocumentRepository::loadAllProfessionalDocuments()
{
    QList<ProfessionalDocument> allProfessionalDocuments;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.exec(QStringLiteral(
        "SELECT * FROM professionalDocument ORDER BY importedTimestamp DESC"));

    while (selectQuery.next()) {
        allProfessionalDocuments.append(professionalDocumentFromQueryRow(selectQuery));
    }
    return allProfessionalDocuments;
}

QList<ProfessionalDocument> ProfessionalDocumentRepository::loadProfessionalDocumentsOfKind(
    const QString &documentKind)
{
    QList<ProfessionalDocument> professionalDocumentsOfKind;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.prepare(QStringLiteral(
        "SELECT * FROM professionalDocument WHERE documentKind = :documentKind "
        "ORDER BY importedTimestamp DESC"));
    selectQuery.bindValue(QStringLiteral(":documentKind"), documentKind);
    selectQuery.exec();

    while (selectQuery.next()) {
        professionalDocumentsOfKind.append(professionalDocumentFromQueryRow(selectQuery));
    }
    return professionalDocumentsOfKind;
}

bool ProfessionalDocumentRepository::updateDocumentKind(qint64 professionalDocumentId,
                                                        const QString &documentKind)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE professionalDocument SET documentKind = :documentKind "
        "WHERE professionalDocumentId = :professionalDocumentId"));
    updateQuery.bindValue(QStringLiteral(":documentKind"), textOrEmpty(documentKind));
    updateQuery.bindValue(QStringLiteral(":professionalDocumentId"), professionalDocumentId);

    if (!updateQuery.exec()) {
        lastErrorDescription = updateQuery.lastError().text();
        return false;
    }
    return true;
}

bool ProfessionalDocumentRepository::removeProfessionalDocument(qint64 professionalDocumentId)
{
    QSqlQuery deleteQuery(jobCrushDatabase.connection());
    deleteQuery.prepare(QStringLiteral(
        "DELETE FROM professionalDocument WHERE professionalDocumentId = :professionalDocumentId"));
    deleteQuery.bindValue(QStringLiteral(":professionalDocumentId"), professionalDocumentId);

    if (!deleteQuery.exec()) {
        lastErrorDescription = deleteQuery.lastError().text();
        return false;
    }
    return true;
}

QString ProfessionalDocumentRepository::allExtractedTextJoined()
{
    QString joinedText;

    QSqlQuery selectQuery(jobCrushDatabase.connection());
    selectQuery.exec(QStringLiteral(
        "SELECT extractedText FROM professionalDocument WHERE extractedText <> ''"));

    while (selectQuery.next()) {
        joinedText += selectQuery.value(0).toString();
        joinedText += QStringLiteral("\n\n");
    }
    return joinedText;
}

QString ProfessionalDocumentRepository::lastErrorText() const
{
    return lastErrorDescription;
}

bool ProfessionalDocumentRepository::markDocumentAsRead(qint64 professionalDocumentId)
{
    QSqlQuery updateQuery(jobCrushDatabase.connection());
    updateQuery.prepare(QStringLiteral(
        "UPDATE professionalDocument SET hasBeenReadForInsights = 1 "
        "WHERE professionalDocumentId = :professionalDocumentId"));
    updateQuery.bindValue(QStringLiteral(":professionalDocumentId"), professionalDocumentId);

    if (!updateQuery.exec()) {
        lastErrorDescription = updateQuery.lastError().text();
        return false;
    }
    return true;
}
