#include "ProfessionalDocumentListViewModel.h"

#include <QDate>
#include <QDesktopServices>
#include <QFile>
#include <QLocale>
#include <QUrl>

#include "../model/ProfessionalDocumentRepository.h"
#include "../modelview/prodocs/DocumentKind.h"
#include "../modelview/prodocs/ProDocsIntake.h"

namespace {

QString importedWhenTextFor(const QDateTime &importedTimestamp)
{
    if (!importedTimestamp.isValid()) {
        return QString();
    }
    const QDate importedDate = importedTimestamp.date();
    const QDate todayDate = QDate::currentDate();
    if (importedDate == todayDate) {
        return QStringLiteral("Added today");
    }
    if (importedDate == todayDate.addDays(-1)) {
        return QStringLiteral("Added yesterday");
    }
    return QStringLiteral("Added %1").arg(QLocale().toString(importedDate,
                                                             QStringLiteral("MMMM d")));
}

QString fileSizeTextFor(qint64 fileSizeBytes)
{
    if (fileSizeBytes <= 0) {
        return QString();
    }
    if (fileSizeBytes < 1024) {
        return QStringLiteral("%1 bytes").arg(fileSizeBytes);
    }
    if (fileSizeBytes < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(fileSizeBytes / 1024);
    }
    return QStringLiteral("%1.%2 MB")
        .arg(fileSizeBytes / (1024 * 1024))
        .arg((fileSizeBytes % (1024 * 1024)) / (1024 * 105));
}

// Word count is the honest measure of whether a document actually loaded.
// "2,140 words" tells the user their resume is in; "no readable text" tells
// them it is not, and no file size or green tick can say either.
QString wordCountTextFor(const QString &extractedText)
{
    if (extractedText.trimmed().isEmpty()) {
        return QStringLiteral("no readable text");
    }
    const int wordCount = extractedText.simplified().count(QLatin1Char(' ')) + 1;
    return QStringLiteral("%1 words").arg(QLocale().toString(wordCount));
}

} // namespace

ProfessionalDocumentListViewModel::ProfessionalDocumentListViewModel(
    ProfessionalDocumentRepository &documentRepository,
    ProDocsIntake &intake,
    QObject *parent)
    : QAbstractListModel(parent)
    , repository(documentRepository)
    , proDocsIntake(intake)
{
    connect(&proDocsIntake, &ProDocsIntake::professionalDocumentsChanged,
            this, [this]() { reloadDocuments(); });
    reloadDocuments();
}

void ProfessionalDocumentListViewModel::reloadDocuments()
{
    beginResetModel();
    loadedDocuments = repository.loadAllProfessionalDocuments();
    endResetModel();
    emit documentsChanged();
}

int ProfessionalDocumentListViewModel::rowCount(const QModelIndex &parentIndex) const
{
    if (parentIndex.isValid()) {
        return 0;
    }
    return loadedDocuments.count();
}

QVariant ProfessionalDocumentListViewModel::data(const QModelIndex &modelIndex, int role) const
{
    if (!modelIndex.isValid() || modelIndex.row() >= loadedDocuments.count()) {
        return QVariant();
    }
    const ProfessionalDocument &professionalDocument = loadedDocuments.at(modelIndex.row());

    switch (role) {
    case DisplayNameRole:       return professionalDocument.displayName;
    case DocumentKindRole:      return professionalDocument.documentKind;
    case DocumentKindLabelRole: return DocumentKind::displayNameFor(professionalDocument.documentKind);
    case ImportedWhenTextRole:  return importedWhenTextFor(professionalDocument.importedTimestamp);
    case FileSizeTextRole:      return fileSizeTextFor(professionalDocument.fileSizeBytes);
    case ExtractionNoteRole:    return professionalDocument.textExtractionNote;
    case HasReadableTextRole:   return !professionalDocument.extractedText.trimmed().isEmpty();
    case WordCountTextRole:     return wordCountTextFor(professionalDocument.extractedText);
    }
    return QVariant();
}

QHash<int, QByteArray> ProfessionalDocumentListViewModel::roleNames() const
{
    return {
        { DisplayNameRole,       QByteArrayLiteral("displayName") },
        { DocumentKindRole,      QByteArrayLiteral("documentKind") },
        { DocumentKindLabelRole, QByteArrayLiteral("documentKindLabel") },
        { ImportedWhenTextRole,  QByteArrayLiteral("importedWhenText") },
        { FileSizeTextRole,      QByteArrayLiteral("fileSizeText") },
        { ExtractionNoteRole,    QByteArrayLiteral("extractionNote") },
        { HasReadableTextRole,   QByteArrayLiteral("hasReadableText") },
        { WordCountTextRole,     QByteArrayLiteral("wordCountText") },
    };
}

int ProfessionalDocumentListViewModel::rowCountForProperty() const
{
    return loadedDocuments.count();
}

QString ProfessionalDocumentListViewModel::lastDropOutcomeText() const
{
    return storedLastDropOutcomeText;
}

void ProfessionalDocumentListViewModel::acceptDroppedFiles(const QStringList &droppedFilePaths)
{
    // A drop hands over URLs ("file:///C:/Users/..."), not paths. Converting
    // here rather than with string surgery in QML is what keeps Windows drive
    // letters, spaces and accented folder names working.
    QStringList localFilePaths;
    localFilePaths.reserve(droppedFilePaths.count());
    for (const QString &droppedFilePath : droppedFilePaths) {
        const QUrl droppedUrl(droppedFilePath);
        localFilePaths.append(droppedUrl.isLocalFile() ? droppedUrl.toLocalFile()
                                                       : droppedFilePath);
    }

    storedLastDropOutcomeText = proDocsIntake.acceptDroppedFiles(localFilePaths);
    emit lastDropOutcomeChanged();
}

QStringList ProfessionalDocumentListViewModel::selectableDocumentKinds() const
{
    return DocumentKind::allDocumentKinds();
}

QString ProfessionalDocumentListViewModel::documentKindLabel(const QString &documentKind) const
{
    return DocumentKind::displayNameFor(documentKind);
}

void ProfessionalDocumentListViewModel::setDocumentKindAt(int rowIndex,
                                                          const QString &documentKind)
{
    if (rowIndex < 0 || rowIndex >= loadedDocuments.count()) {
        return;
    }
    if (repository.updateDocumentKind(
            loadedDocuments.at(rowIndex).professionalDocumentId, documentKind)) {
        reloadDocuments();
    }
}

void ProfessionalDocumentListViewModel::removeDocumentAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedDocuments.count()) {
        return;
    }
    const ProfessionalDocument professionalDocument = loadedDocuments.at(rowIndex);

    // Only Job Crush's own copy goes. The user's file was never ours to
    // delete, and deleting somebody's only resume would be unforgivable.
    if (!professionalDocument.storedFilePath.isEmpty()) {
        QFile::remove(professionalDocument.storedFilePath);
    }
    if (repository.removeProfessionalDocument(professionalDocument.professionalDocumentId)) {
        reloadDocuments();
    }
}

void ProfessionalDocumentListViewModel::openDocumentAt(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= loadedDocuments.count()) {
        return;
    }
    const QString storedFilePath = loadedDocuments.at(rowIndex).storedFilePath;
    if (storedFilePath.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(storedFilePath));
}

void ProfessionalDocumentListViewModel::rereadEveryDocument()
{
    // The same one-sentence channel a drop uses, because it answers the same
    // question: what did Job Crush just do with my documents?
    storedLastDropOutcomeText = proDocsIntake.rereadEveryDocument();
    emit lastDropOutcomeChanged();
}
