#include "ProfessionalDocumentListViewModel.h"

#include "../logic/prodocs/MoonlightDocumentCleanup.h"

#include <QDate>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QUrl>

#include "../model/ProfessionalDocumentRepository.h"
#include "../logic/AppPreferences.h"
#include "../logic/exporting/ExportFolder.h"
#include "../logic/exporting/ExportFormat.h"
#include "../logic/prodocs/DocumentKind.h"
#include "../logic/prodocs/ProDocsIntake.h"

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

const qint64 bytesPerKilobyte = 1024;
const qint64 bytesPerMegabyte = 1024 * 1024;

// Used to get the one digit after the point in "2.3 MB". A true tenth of a
// megabyte is 104,857.6 bytes. This is 1024 * 105 = 107,520 bytes, a little
// more than a true tenth, so the digit can read one lower than it should
// near the top of each tenth. It never reaches 10.
const qint64 bytesPerTenthOfMegabyte = 1024 * 105;

// "812 bytes", "54 KB", "2.3 MB". Each step rounds down.
// Under a kilobyte: the exact byte count.
// Under a megabyte: whole kilobytes.
// Otherwise: whole megabytes, a point, then the leftover bytes past the
// whole megabytes divided into tenths, as one digit.
QString fileSizeTextFor(qint64 fileSizeBytes)
{
    if (fileSizeBytes <= 0) {
        return QString();
    }
    if (fileSizeBytes < bytesPerKilobyte) {
        return QStringLiteral("%1 bytes").arg(fileSizeBytes);
    }
    if (fileSizeBytes < bytesPerMegabyte) {
        return QStringLiteral("%1 KB").arg(fileSizeBytes / bytesPerKilobyte);
    }
    return QStringLiteral("%1.%2 MB")
        .arg(fileSizeBytes / bytesPerMegabyte)
        .arg((fileSizeBytes % bytesPerMegabyte) / bytesPerTenthOfMegabyte);
}

// The word count tells the user whether a document really loaded.
// "2,140 words" says their resume is in. "no readable text" says it is not.
// A file size or a green tick cannot tell them either thing.
//
// How the count works: simplified() turns every run of spaces, tabs and
// newlines into one space and trims both ends. The words are then the
// spaces plus one.
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
    AppPreferences &preferences,
    MoonlightDocumentCleanup &documentCleanup,
    const QString &dataFolderPath,
    QObject *parent)
    : QAbstractListModel(parent)
    , repository(documentRepository)
    , proDocsIntake(intake)
    , appPreferences(preferences)
    , moonlightCleanup(documentCleanup)
    , applicationDataFolderPath(dataFolderPath)
{
    connect(&moonlightCleanup, &MoonlightDocumentCleanup::cleanupStatusChanged, this,
            [this](const QString &outcomeText) {
        reportOutcome(outcomeText);
        emit cleanupChanged();
    });
    // The brain may have been connected since the page last asked.
    connect(this, &ProfessionalDocumentListViewModel::documentsChanged,
            this, &ProfessionalDocumentListViewModel::cleanupChanged);
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

QString ProfessionalDocumentListViewModel::lastProDocsOutcomeText() const
{
    return storedLastProDocsOutcomeText;
}

void ProfessionalDocumentListViewModel::reportOutcome(const QString &outcomeText)
{
    storedLastProDocsOutcomeText = outcomeText;
    emit lastProDocsOutcomeChanged();
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

    reportOutcome(proDocsIntake.acceptDroppedFiles(localFilePaths));
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
    const qint64 professionalDocumentId = loadedDocuments.at(rowIndex).professionalDocumentId;
    if (!repository.updateDocumentKind(professionalDocumentId, documentKind)) {
        qWarning().noquote() << QStringLiteral(
            "ProfessionalDocumentListViewModel::setDocumentKindAt: could not set kind "
            "\"%1\" on document %2 — %3")
                .arg(documentKind).arg(professionalDocumentId).arg(repository.lastErrorText());
        return;
    }
    reloadDocuments();
}

void ProfessionalDocumentListViewModel::removeDocumentAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedDocuments.count()) {
        return;
    }
    const ProfessionalDocument professionalDocument = loadedDocuments.at(rowIndex);

    // Only Job Crush's own copy goes. The user's original file is never
    // deleted. It might be their only copy of their resume.
    if (!professionalDocument.storedFilePath.isEmpty()
            && !QFile::remove(professionalDocument.storedFilePath)) {
        qWarning().noquote() << QStringLiteral(
            "ProfessionalDocumentListViewModel::removeDocumentAt: could not delete Job "
            "Crush's copy \"%1\" of document %2 — the file may be missing or in use")
                .arg(professionalDocument.storedFilePath)
                .arg(professionalDocument.professionalDocumentId);
    }
    if (!repository.removeProfessionalDocument(professionalDocument.professionalDocumentId)) {
        qWarning().noquote() << QStringLiteral(
            "ProfessionalDocumentListViewModel::removeDocumentAt: could not remove "
            "document %1 — %2")
                .arg(professionalDocument.professionalDocumentId)
                .arg(repository.lastErrorText());
        return;
    }
    reloadDocuments();
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
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(storedFilePath))) {
        qWarning().noquote() << QStringLiteral(
            "ProfessionalDocumentListViewModel::openDocumentAt: could not open \"%1\" — "
            "the system refused it").arg(storedFilePath);
    }
}

void ProfessionalDocumentListViewModel::rereadEveryDocument()
{
    // The same one-sentence channel a drop uses, because it answers the same
    // question: what did Job Crush just do with my documents?
    reportOutcome(proDocsIntake.rereadEveryDocument());
}

QStringList ProfessionalDocumentListViewModel::selectableConversionFormats() const
{
    return ExportFormat::everyConversionFormat();
}

QString ProfessionalDocumentListViewModel::conversionFormatButtonName(
    const QString &format) const
{
    return ExportFormat::buttonNameFor(format);
}

QString ProfessionalDocumentListViewModel::warningAboutConversion() const
{
    return QStringLiteral("You get the words, not the page design — a two-column "
                          "resume comes out as one column. Your original is untouched.");
}

void ProfessionalDocumentListViewModel::saveCopyOfDocumentAt(int rowIndex,
                                                             const QString &format)
{
    if (rowIndex < 0 || rowIndex >= loadedDocuments.count()) {
        return;
    }

    const ProfessionalDocument &document = loadedDocuments.at(rowIndex);

    // The name the user already knows this document by, without its old
    // extension, and with the characters Windows refuses in a file name taken
    // out. The pattern is one character class holding a backslash, then
    // / : * ? " < > |. The backslash is written four times over because both
    // C++ and the pattern need it escaped.
    QString baseName = QFileInfo(document.displayName).completeBaseName();
    baseName.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    baseName = baseName.trimmed();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("Job Crush document");
    }

    const DocumentConverter::ConversionOutcome outcome = documentConverter.convertToFile(
        document.extractedText,
        baseName,
        format,
        ExportFolder::folderJobCrushWritesTo(appPreferences, applicationDataFolderPath));

    if (!outcome.succeeded) {
        reportOutcome(outcome.reasonText + QStringLiteral("  ") + outcome.whatToDoNextText);
        return;
    }

    // The full path, not only "saved", so the user can go and find the file.
    reportOutcome(QStringLiteral("Saved a %1 copy: %2")
                      .arg(ExportFormat::displayNameFor(format),
                           QDir::toNativeSeparators(outcome.writtenFilePath)));
}

bool ProfessionalDocumentListViewModel::brainCanCleanUp() const
{
    return moonlightCleanup.aBrainIsAvailable();
}

bool ProfessionalDocumentListViewModel::cleanupIsRunning() const
{
    return moonlightCleanup.cleanupIsRunning();
}

void ProfessionalDocumentListViewModel::cleanUpDocumentAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedDocuments.count()) {
        return;
    }
    const ProfessionalDocument &document = loadedDocuments.at(rowIndex);
    moonlightCleanup.cleanUpDocument(document.professionalDocumentId, document.displayName,
                                     document.extractedText);
}

void ProfessionalDocumentListViewModel::layOutDocumentAt(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= loadedDocuments.count()) {
        return;
    }
    const ProfessionalDocument &document = loadedDocuments.at(rowIndex);
    moonlightCleanup.layOutDocument(
        document.displayName, document.extractedText,
        ExportFolder::folderJobCrushWritesTo(appPreferences, applicationDataFolderPath));
}
