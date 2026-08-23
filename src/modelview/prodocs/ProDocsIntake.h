#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "DocumentTextExtractor.h"
#include "DroppedFileClassifier.h"
#include "ResumeInsightParser.h"

class CareerHistoryRepository;
class ProfessionalDocumentRepository;

// ProDocsIntake
//
// The front door of ProDocs — a ModelView resident. Everything above it hands
// over dropped file paths and never learns where documents are kept, how text
// comes out of a PDF, or how a resume is recognised.
//
// What one drop does, in order:
//   1. refuse anything that isn't a document or an image, out loud
//   2. copy the file into Job Crush's own folder, because the user's copy is
//      theirs to move, rename or delete
//   3. pull out the text, or say honestly why there isn't any
//   4. work out what kind of document it is
//   5. store the record
//
// Nothing here is automated in the sense the plan forbids: files arrive only
// because a person dragged them in, and every guess it makes is one the user
// can correct.
class ProDocsIntake : public QObject {
    Q_OBJECT
public:
    ProDocsIntake(ProfessionalDocumentRepository &documentRepository,
                  CareerHistoryRepository &careerRepository,
                  const QString &applicationDataFolderPath,
                  QObject *parent = nullptr);

    // Takes however many files were dropped at once. Returns a short, human
    // sentence about what happened — the drop basket shows it verbatim.
    QString acceptDroppedFiles(const QStringList &droppedFilePaths);

    // Everything ProDocs has read, joined. The search profile tops itself up
    // from this, which is how documents make job matching sharper.
    QString allDocumentTextJoined() const;

    // --- When the reader itself gets better -------------------------------
    //
    // Job Crush's resume reader improves between builds, and the entries in
    // the database were produced by whichever version happened to be running
    // that day. A user who dropped a resume in August should not be stuck
    // forever with August's mistakes — "Tulsa Community College 2000 - 2001
    // Auburn University 1993 - 1994" sitting in a school box because that
    // build could not tell two schools apart.
    //
    // So the reader carries a version. When the running one is newer than the
    // one that last read this database, everything the READER produced and
    // nobody has typed into is thrown away, every document is marked unread,
    // and they are read again by the better reader. Words the user typed are
    // never touched. Returns how many entries the fresh reading produced, or
    // 0 when the reader has not changed and there is nothing to do.
    int rereadEverythingIfTheReaderImproved();

    // Reads any document that has never been read, and nothing else.
    //
    // Called once at startup so documents dropped before this feature existed
    // — or dropped while the page was closed — turn up without the user
    // having to know a button exists. Documents already read are left alone,
    // so entries somebody deleted on purpose stay deleted.
    //
    // Returns how many entries it found.
    int readAnyDocumentsNotReadYet();

    // Reads every stored document again from scratch.
    //
    // Anything the user confirmed or typed themselves SURVIVES this. Re-reading
    // must never quietly undo a correction somebody made by hand — that would
    // punish them for helping.
    //
    // Returns a plain sentence about what it found, for the page to show.
    QString rereadEveryDocument();

signals:
    // A document arrived, was reclassified, or was removed.
    void professionalDocumentsChanged();

    // Jobs or schooling were read out of a document, or re-read.
    void careerHistoryChanged();

private:
    // Copies into <appData>/prodocs, never overwriting: a second "resume.pdf"
    // becomes "resume (2).pdf" rather than quietly replacing the first, which
    // would destroy a document the user still believes they have.
    QString copyIntoDocumentStorage(const QString &sourceFilePath) const;

    // Reads one document's text into work experience and education entries.
    // Returns how many entries came out of it.
    int recordInsightsFromDocument(qint64 professionalDocumentId,
                                   const QString &documentText);

    ProfessionalDocumentRepository &repository;
    CareerHistoryRepository &careerHistoryRepository;
    const QString documentStorageFolderPath;

    DocumentTextExtractor textExtractor;
    DroppedFileClassifier fileClassifier;
    ResumeInsightParser resumeParser;
};
