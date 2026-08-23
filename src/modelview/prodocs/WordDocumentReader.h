#pragma once

#include <QString>

// WordDocumentReader
//
// Reads the words out of a .docx file.
//
// A .docx is a ZIP archive whose word/document.xml holds the text, wrapped in
// a great deal of formatting markup. This pulls out the text and the
// paragraph breaks and throws the rest away — the styling matters to Word,
// not to anything Job Crush does with the words.
class WordDocumentReader {
public:
    // The document's readable text. Empty when the file could not be read;
    // reasonText then says why, in words meant for the user.
    QString readTextFrom(const QString &wordDocumentFilePath, QString &reasonText) const;
};
