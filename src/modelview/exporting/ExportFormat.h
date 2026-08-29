#pragma once

#include <QString>
#include <QStringList>

// ExportFormat
//
// The formats a packet can be exported as.
//
// Markdown is deliberately not on this list. It is the working format inside
// the app because it edits and converts cleanly. It is not a format to hand
// someone who wants to print a cover letter. (Decided 2026-08-23.)
namespace ExportFormat {

inline const QString WordDocument = QStringLiteral("docx");
inline const QString PortableDocument = QStringLiteral("pdf");

// Plain text is a CONVERSION target only, never a packet format. Nobody sends
// a cover letter as a .txt file. It earns its place when somebody needs the
// words out of a document and into something else entirely.
inline const QString PlainTextDocument = QStringLiteral("txt");

// The choices, in the order they are shown. Word is first because most
// application systems accept and read .docx files.
inline QStringList everyFormat()
{
    return { WordDocument, PortableDocument };
}

// The formats a document already in ProDocs can be saved out as. Wider than
// everyFormat() by exactly one: see PlainTextDocument above.
inline QStringList everyConversionFormat()
{
    return { WordDocument, PortableDocument, PlainTextDocument };
}

inline QString displayNameFor(const QString &format)
{
    if (format == WordDocument)      return QStringLiteral("Word (.docx)");
    if (format == PortableDocument)  return QStringLiteral("PDF");
    if (format == PlainTextDocument) return QStringLiteral("Plain text (.txt)");
    return QStringLiteral("Word (.docx)");
}

// Short enough to sit on a button. "Word", "PDF", "Text".
inline QString buttonNameFor(const QString &format)
{
    if (format == PortableDocument)  return QStringLiteral("PDF");
    if (format == PlainTextDocument) return QStringLiteral("Text");
    return QStringLiteral("Word");
}

// The explanation under each choice. Both are true. Which one matters depends
// on where the application is going, so the app shows both and lets the user
// pick.
inline QString explanationFor(const QString &format)
{
    if (format == WordDocument) {
        return QStringLiteral("Editable, and what most application systems ask for.");
    }
    if (format == PortableDocument) {
        return QStringLiteral("Looks the same everywhere, and can't be edited by accident.");
    }
    if (format == PlainTextDocument) {
        return QStringLiteral("Just the words, for pasting into a form or an email.");
    }
    return QString();
}

inline QString fileExtensionFor(const QString &format)
{
    if (format == PortableDocument)  return QStringLiteral("pdf");
    if (format == PlainTextDocument) return QStringLiteral("txt");
    return QStringLiteral("docx");
}

// A packet format. Deliberately narrower than isKnownConversionFormat: a
// preference holding "txt" must not turn a sent application into a text file.
inline bool isKnownFormat(const QString &format)
{
    return format == WordDocument || format == PortableDocument;
}

inline bool isKnownConversionFormat(const QString &format)
{
    return isKnownFormat(format) || format == PlainTextDocument;
}

} // namespace ExportFormat
