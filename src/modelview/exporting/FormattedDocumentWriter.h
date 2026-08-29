#pragma once

#include <QString>

#include "MarkdownDocumentReader.h"

// FormattedDocumentWriter
//
// Writes the app's internal markdown out as a real .docx or a real PDF.
//
// This was inside PacketExporter, which is where it was first needed. It moved
// out the day a second caller appeared: converting a document a user dropped
// on ProDocs is the same job — markdown in, a file somebody else can open out
// — and two copies of a Word writer is one copy too many.
//
// A ModelView class. It knows document formats and nothing about screens.
class FormattedDocumentWriter {
public:
    // Both return false and fill reasonText when the file could not be
    // written. reasonText is for a person to read.
    bool writeWordDocument(const QString &markdownText,
                           const QString &filePath,
                           QString &reasonText) const;

    bool writePortableDocument(const QString &markdownText,
                               const QString &filePath,
                               QString &reasonText) const;

private:
    MarkdownDocumentReader markdownReader;
};
