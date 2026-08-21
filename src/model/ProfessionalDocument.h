#pragma once

#include <QDateTime>
#include <QString>

// ProfessionalDocument
//
// One item in ProDocs: a resume, transcript, certification, reference, etc.
// The original file stays where the user keeps it; we remember where it was
// and keep the extracted text, which is what AIBrain actually reads.
struct ProfessionalDocument {
    qint64 professionalDocumentId = 0;    // database identity; 0 means "not saved yet"
    QString documentKind;                 // "resume" | "transcript" | "certification" | "reference" | "other"
    QString displayName;                  // what the user sees in the ProDocs list
    QString originalFilePath;             // where the dropped file lived at import time
    QString extractedText;                // clean text pulled from the file at import
    QDateTime importedTimestamp;
};
