#pragma once

#include <QDateTime>
#include <QString>

// ProfessionalDocument
//
// One item in ProDocs: a resume, transcript, certification, reference, photo.
//
// Job Crush keeps its OWN copy of every document. The user's file stays
// wherever they keep it and they are free to move, rename or delete it — the
// app's filing is the app's to manage. If the app pointed at the user's own
// file, tidying a Downloads folder would break it.
struct ProfessionalDocument {
    qint64 professionalDocumentId = 0;    // database identity; 0 means "not saved yet"
    QString documentKind;                 // see src/logic/prodocs/DocumentKind.h
    QString displayName;                  // what the user sees in the ProDocs list
    QString originalFilePath;             // where it lived when it was dropped
    QString storedFilePath;               // Job Crush's own copy
    QString extractedText;                // clean text pulled from the file at import
    QDateTime importedTimestamp;
    qint64 fileSizeBytes = 0;

    // Why there is no text, when there is no text. A scanned resume is a
    // picture of words, and saying so plainly is the difference between the
    // user knowing their experience did not load and quietly assuming it did.
    QString textExtractionNote;

    // Whether jobs and schooling have already been read out of this one.
    //
    // Needed so a document is read automatically ONCE. Re-reading everything
    // on every launch would bring back entries the user had deleted on
    // purpose.
    bool hasBeenReadForInsights = false;
};
