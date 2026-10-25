#include "ProDocsIntake.h"

#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "../../model/CareerHistoryRepository.h"
#include "../../model/ProfessionalDocumentRepository.h"
#include "DocumentKind.h"
#include "DroppedFileTypes.h"

namespace {

// THE READER'S VERSION. Bump this whenever ResumeInsightParser learns
// something — a new heading, a smarter split, a bug fixed — and every user's
// documents are read again by the better reader on their next launch,
// replacing what the old one got wrong.
//
//   1  the first reader that shipped
//   2  fixed: non-breaking spaces made every resume line one unsplittable
//      word, so two schools on one line became one school with a strange
//      name. Explicit "|" field separators are no longer second-guessed, and
//      a run of spaces is now read as the column gap it is.
//   3, 4  later reader fixes. What changed in each was not written down
//      here. Add a line for every new version.
constexpr int currentInsightsReaderVersion = 4;

// Where the last-used reader version is remembered. QSettings alongside the
// rest of the app's preferences — this is a fact about the installation, not
// about any one document.
const QString insightsReaderVersionSettingsKey =
    QStringLiteral("proDocs/insightsReaderVersion");

} // namespace

ProDocsIntake::ProDocsIntake(ProfessionalDocumentRepository &documentRepository,
                             CareerHistoryRepository &careerRepository,
                             const QString &applicationDataFolderPath,
                             QObject *parent)
    : QObject(parent)
    , repository(documentRepository)
    , careerHistoryRepository(careerRepository)
    , documentStorageFolderPath(
          QDir(applicationDataFolderPath).filePath(QStringLiteral("prodocs")))
{
    if (!QDir().mkpath(documentStorageFolderPath)) {
        qWarning().noquote() << QStringLiteral(
            "ProDocsIntake::ProDocsIntake: could not create the document folder \"%1\"")
                .arg(documentStorageFolderPath);
    }
}

QString ProDocsIntake::copyIntoDocumentStorage(const QString &sourceFilePath) const
{
    const QFileInfo sourceFileInfo(sourceFilePath);
    const QString baseName = sourceFileInfo.completeBaseName();
    const QString extension = sourceFileInfo.suffix();

    QDir storageFolder(documentStorageFolderPath);
    QString candidateFileName = sourceFileInfo.fileName();

    // Never overwrite. Someone dropping a second "resume.pdf" is almost always
    // adding a new version, not throwing the old one away — and silently
    // replacing a document they still think they have is unrecoverable.
    int duplicateNumber = 2;
    while (storageFolder.exists(candidateFileName)) {
        candidateFileName = extension.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(baseName).arg(duplicateNumber)
            : QStringLiteral("%1 (%2).%3").arg(baseName).arg(duplicateNumber).arg(extension);
        ++duplicateNumber;
    }

    const QString destinationFilePath = storageFolder.filePath(candidateFileName);
    if (!QFile::copy(sourceFilePath, destinationFilePath)) {
        qWarning().noquote() << QStringLiteral(
            "ProDocsIntake::copyIntoDocumentStorage: could not copy \"%1\" to \"%2\"")
                .arg(sourceFilePath, destinationFilePath);
        return QString();
    }
    return destinationFilePath;
}

QString ProDocsIntake::acceptDroppedFiles(const QStringList &droppedFilePaths)
{
    int acceptedCount = 0;
    QStringList refusalReasons;
    QStringList extractionNotes;

    for (const QString &droppedFilePath : droppedFilePaths) {
        if (!QFileInfo::exists(droppedFilePath)) {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::acceptDroppedFiles: skipped \"%1\" — the file does not exist")
                    .arg(droppedFilePath);
            continue;
        }

        if (!DroppedFileTypes::isAccepted(droppedFilePath)) {
            refusalReasons.append(
                DroppedFileTypes::reasonFileCannotBeAccepted(droppedFilePath));
            continue;
        }

        const QString storedFilePath = copyIntoDocumentStorage(droppedFilePath);
        if (storedFilePath.isEmpty()) {
            refusalReasons.append(
                QStringLiteral("Job Crush couldn't make its own copy of %1.")
                    .arg(QFileInfo(droppedFilePath).fileName()));
            continue;
        }

        // Read OUR copy from here on. The user's file is theirs again the
        // moment it is copied, and may be gone a second later.
        const DocumentTextExtractor::ExtractionResult extraction =
            textExtractor.extractTextFrom(storedFilePath);

        ProfessionalDocument professionalDocument;
        professionalDocument.documentKind =
            fileClassifier.classifyDocument(droppedFilePath, extraction.extractedText);
        professionalDocument.displayName = QFileInfo(droppedFilePath).fileName();
        professionalDocument.originalFilePath = droppedFilePath;
        professionalDocument.storedFilePath = storedFilePath;
        professionalDocument.extractedText = extraction.extractedText;
        professionalDocument.textExtractionNote = extraction.note;
        professionalDocument.importedTimestamp = QDateTime::currentDateTime();
        professionalDocument.fileSizeBytes = QFileInfo(storedFilePath).size();

        if (!repository.insertProfessionalDocument(professionalDocument)) {
            refusalReasons.append(
                QStringLiteral("Job Crush couldn't file %1 — %2")
                    .arg(professionalDocument.displayName, repository.lastErrorText()));
            continue;
        }

        ++acceptedCount;

        const int entriesRead = recordInsightsFromDocument(
            professionalDocument.professionalDocumentId, extraction.extractedText,
            professionalDocument.documentKind);
        if (!repository.markDocumentAsRead(professionalDocument.professionalDocumentId)) {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::acceptDroppedFiles: could not mark document %1 as read — %2")
                    .arg(professionalDocument.professionalDocumentId)
                    .arg(repository.lastErrorText());
        }
        if (entriesRead > 0) {
            emit careerHistoryChanged();
        }

        if (!extraction.note.isEmpty()) {
            extractionNotes.append(QStringLiteral("%1: %2")
                                       .arg(professionalDocument.displayName, extraction.note));
        }
    }

    if (acceptedCount > 0) {
        emit professionalDocumentsChanged();
    }

    // One sentence about what actually happened. Both halves matter: a drop
    // that half worked must not read as a clean success.
    QStringList spokenParts;
    if (acceptedCount > 0) {
        spokenParts.append(acceptedCount == 1
            ? QStringLiteral("Got it — filed under ProDocs.")
            : QStringLiteral("Got them — %1 files filed under ProDocs.").arg(acceptedCount));
    }
    spokenParts += extractionNotes;
    spokenParts += refusalReasons;

    if (spokenParts.isEmpty()) {
        return QStringLiteral("Nothing came through — try dropping the file again.");
    }
    return spokenParts.join(QStringLiteral("  "));
}

QString ProDocsIntake::allDocumentTextJoined() const
{
    return repository.allExtractedTextJoined();
}

int ProDocsIntake::recordInsightsFromDocument(qint64 professionalDocumentId,
                                              const QString &documentText,
                                              const QString &documentKind)
{
    if (documentText.trimmed().isEmpty()) {
        return 0;
    }

    // A transcript goes to the reader that knows what one looks like. If that
    // reader does not recognise it — a document filed as a transcript that
    // turns out to be something else, which classification gets wrong often
    // enough to matter — the resume parser gets its turn rather than the user
    // getting nothing.
    ParsedResumeInsights parsedInsights;
    if (documentKind == DocumentKind::Transcript) {
        parsedInsights = transcriptReader.parseTranscriptText(documentText);
    }
    if (parsedInsights.educationRecords.isEmpty()
            && parsedInsights.workExperiences.isEmpty()) {
        parsedInsights = resumeParser.parseResumeText(documentText);
    }
    int storedEntryCount = 0;

    // Reading a document twice must not say everything twice.
    //
    // This is the half that was missing. "Read my documents again" clears the
    // entries Job Crush guessed and keeps the ones the user confirmed — which
    // is right — and then re-read every document and created the confirmed
    // ones all over again. Every entry the user had ticked ended up with an
    // identical unconfirmed twin sitting under it, and a second press made a
    // third. Skipping a line already on record from this same document ends
    // that, and leaves genuine hand-typed entries (which carry no source
    // line) completely alone.
    for (WorkExperience workExperience : parsedInsights.workExperiences) {
        workExperience.sourceDocumentId = professionalDocumentId;
        workExperience.isConfirmedByUser = false; // a reading, not a fact
        if (careerHistoryRepository.workExperienceAlreadyRecorded(workExperience)) {
            continue;
        }
        if (careerHistoryRepository.insertWorkExperience(workExperience)) {
            ++storedEntryCount;
        } else {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::recordInsightsFromDocument: could not store a job from"
                " document %1 — %2")
                    .arg(professionalDocumentId)
                    .arg(careerHistoryRepository.lastErrorText());
        }
    }
    for (EducationRecord educationRecord : parsedInsights.educationRecords) {
        educationRecord.sourceDocumentId = professionalDocumentId;
        educationRecord.isConfirmedByUser = false;
        if (careerHistoryRepository.educationRecordAlreadyRecorded(educationRecord)) {
            continue;
        }
        if (careerHistoryRepository.insertEducationRecord(educationRecord)) {
            ++storedEntryCount;
        } else {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::recordInsightsFromDocument: could not store a school from"
                " document %1 — %2")
                    .arg(professionalDocumentId)
                    .arg(careerHistoryRepository.lastErrorText());
        }
    }
    return storedEntryCount;
}

int ProDocsIntake::readAnyDocumentsNotReadYet()
{
    int entriesFound = 0;
    for (const ProfessionalDocument &professionalDocument :
             repository.loadAllProfessionalDocuments()) {
        if (professionalDocument.hasBeenReadForInsights
                || professionalDocument.extractedText.trimmed().isEmpty()) {
            continue;
        }
        entriesFound += recordInsightsFromDocument(
            professionalDocument.professionalDocumentId, professionalDocument.extractedText,
            professionalDocument.documentKind);
        if (!repository.markDocumentAsRead(professionalDocument.professionalDocumentId)) {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::readAnyDocumentsNotReadYet: could not mark document %1 as"
                " read — %2")
                    .arg(professionalDocument.professionalDocumentId)
                    .arg(repository.lastErrorText());
        }
    }

    if (entriesFound > 0) {
        emit careerHistoryChanged();
    }
    return entriesFound;
}

int ProDocsIntake::rereadEverythingIfTheReaderImproved()
{
    QSettings settings;
    const int readerVersionThatLastRead =
        settings.value(insightsReaderVersionSettingsKey, 0).toInt();

    if (readerVersionThatLastRead >= currentInsightsReaderVersion) {
        return 0; // same reader, same answers — nothing to be gained
    }

    // Record the new version FIRST. If something below fails, the user gets
    // one disappointing startup rather than a re-read that tries and fails on
    // every launch from now on.
    settings.setValue(insightsReaderVersionSettingsKey, currentInsightsReaderVersion);

    // Out with the old reader's work — and nothing else. Entries somebody
    // typed into carry wasEditedByUser and are left exactly where they are.
    if (!careerHistoryRepository.removeEntriesTheReaderProducedAndNobodyTouched()) {
        qWarning().noquote() << QStringLiteral(
            "ProDocsIntake::rereadEverythingIfTheReaderImproved: could not clear the old"
            " reader's entries for reader version %1 — %2")
                .arg(currentInsightsReaderVersion)
                .arg(careerHistoryRepository.lastErrorText());
        return 0;
    }
    if (!repository.markEveryDocumentUnread()) {
        qWarning().noquote() << QStringLiteral(
            "ProDocsIntake::rereadEverythingIfTheReaderImproved: could not mark the"
            " documents unread for reader version %1 — %2")
                .arg(currentInsightsReaderVersion)
                .arg(repository.lastErrorText());
        return 0;
    }

    return readAnyDocumentsNotReadYet();
}

QString ProDocsIntake::rereadEveryDocument()
{
    // Throw away only what Job Crush guessed. Confirmed and hand-typed entries
    // are the user's work and survive untouched.
    if (!careerHistoryRepository.removeUnconfirmedEntries()) {
        return QStringLiteral("Job Crush couldn't clear its earlier readings — %1")
            .arg(careerHistoryRepository.lastErrorText());
    }

    int documentsRead = 0;
    int entriesFound = 0;
    for (const ProfessionalDocument &professionalDocument :
             repository.loadAllProfessionalDocuments()) {
        if (professionalDocument.extractedText.trimmed().isEmpty()) {
            continue;
        }
        ++documentsRead;
        entriesFound += recordInsightsFromDocument(
            professionalDocument.professionalDocumentId, professionalDocument.extractedText,
            professionalDocument.documentKind);
        if (!repository.markDocumentAsRead(professionalDocument.professionalDocumentId)) {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::rereadEveryDocument: could not mark document %1 as read — %2")
                    .arg(professionalDocument.professionalDocumentId)
                    .arg(repository.lastErrorText());
        }
    }

    emit careerHistoryChanged();

    if (documentsRead == 0) {
        return QStringLiteral("There's nothing to read yet. Drag a resume onto the "
                              "window and Job Crush will take it from there.");
    }
    if (entriesFound == 0) {
        return QStringLiteral("Read %1 %2 but couldn't pick out any jobs or schooling. "
                              "Resumes vary a lot — add what's missing below by hand and "
                              "Job Crush will use it exactly the same way.")
            .arg(documentsRead)
            .arg(documentsRead == 1 ? QStringLiteral("document") : QStringLiteral("documents"));
    }
    return QStringLiteral("Read %1 %2 and found %3 %4. Check them over below — anything "
                          "wrong can be fixed right where it sits.")
        .arg(documentsRead)
        .arg(documentsRead == 1 ? QStringLiteral("document") : QStringLiteral("documents"))
        .arg(entriesFound)
        .arg(entriesFound == 1 ? QStringLiteral("entry") : QStringLiteral("entries"));
}

int ProDocsIntake::replaceReadingOfDocument(qint64 professionalDocumentId,
                                            const QList<WorkExperience> &workExperiences,
                                            const QList<EducationRecord> &educationRecords)
{
    if (!careerHistoryRepository.removeUntouchedEntriesFromDocument(professionalDocumentId)) {
        qWarning().noquote() << QStringLiteral(
            "ProDocsIntake::replaceReadingOfDocument: could not remove the old entries of"
            " document %1 — %2")
                .arg(professionalDocumentId)
                .arg(careerHistoryRepository.lastErrorText());
    }

    int entriesAdded = 0;
    for (WorkExperience workExperience : workExperiences) {
        workExperience.sourceDocumentId = professionalDocumentId;
        if (careerHistoryRepository.workExperienceAlreadyRecorded(workExperience)) {
            continue;
        }
        if (careerHistoryRepository.insertWorkExperience(workExperience)) {
            ++entriesAdded;
        } else {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::replaceReadingOfDocument: could not store a job from"
                " document %1 — %2")
                    .arg(professionalDocumentId)
                    .arg(careerHistoryRepository.lastErrorText());
        }
    }
    for (EducationRecord educationRecord : educationRecords) {
        educationRecord.sourceDocumentId = professionalDocumentId;
        if (careerHistoryRepository.educationRecordAlreadyRecorded(educationRecord)) {
            continue;
        }
        if (careerHistoryRepository.insertEducationRecord(educationRecord)) {
            ++entriesAdded;
        } else {
            qWarning().noquote() << QStringLiteral(
                "ProDocsIntake::replaceReadingOfDocument: could not store a school from"
                " document %1 — %2")
                    .arg(professionalDocumentId)
                    .arg(careerHistoryRepository.lastErrorText());
        }
    }
    emit careerHistoryChanged();
    return entriesAdded;
}
