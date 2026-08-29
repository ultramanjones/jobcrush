#pragma once

#include <QString>

#include "ExportFormat.h"
#include "FormattedDocumentWriter.h"

// DocumentConverter
//
// Saves a document Job Crush already holds as a different kind of file: a PDF
// resume out as .docx, a Word one out as PDF, either one out as plain text.
//
// Somebody is asked for a Word copy and all they have is a PDF. Today that
// means searching the web for a converter, landing on whichever ad-funded site
// paid for the top result, and handing a document carrying their address and
// their whole work history to a stranger. Job Crush already reads those files
// and already writes those files. Nothing here is new work; it is two ends
// being joined.
//
// It runs with no AI brain connected, offline, and the document never leaves
// the machine. That last part is the point.
//
// What it does NOT do is reproduce the page. The words come out of the source
// and a NEW document is built from them, so fonts, columns, margins and rules
// are gone: a two-column resume becomes one column of text. That is usually
// what the person asking for a Word copy wanted, and it must never be a
// surprise, so the screen says so before the button is pressed.
//
// A ModelView class. It knows documents and file formats, and nothing about
// screens.
class DocumentConverter {
public:
    struct ConversionOutcome {
        bool succeeded = false;
        QString writtenFilePath;
        QString reasonText;        // why not, in words for a person
        QString whatToDoNextText;  // and what they can do about it
    };

    // documentText is the text Job Crush already read out of the document, so
    // nothing is read twice and a converter is never handed a file format.
    //
    // suggestedBaseName is used as given, without an extension; the caller has
    // already removed characters the filesystem rejects. An existing file is
    // never overwritten - "resume.docx" next to one becomes "resume (2).docx".
    ConversionOutcome convertToFile(const QString &documentText,
                                    const QString &suggestedBaseName,
                                    const QString &format,
                                    const QString &destinationFolderPath) const;

    // Turns the flat text of a document back into the app's markdown, so the
    // Word and PDF writers have the structure they expect.
    //
    // Public because it is worth testing on its own, and because the same
    // rules decide what the user will see.
    static QString markdownForPlainText(const QString &plainText);

private:
    FormattedDocumentWriter documentWriter;
};
