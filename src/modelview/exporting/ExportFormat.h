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

// The choices, in the order they are shown. Word is first because most
// application systems accept and read .docx files.
inline QStringList everyFormat()
{
    return { WordDocument, PortableDocument };
}

inline QString displayNameFor(const QString &format)
{
    if (format == WordDocument)     return QStringLiteral("Word (.docx)");
    if (format == PortableDocument) return QStringLiteral("PDF");
    return QStringLiteral("Word (.docx)");
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
    return QString();
}

inline QString fileExtensionFor(const QString &format)
{
    if (format == PortableDocument) return QStringLiteral("pdf");
    return QStringLiteral("docx");
}

inline bool isKnownFormat(const QString &format)
{
    return format == WordDocument || format == PortableDocument;
}

} // namespace ExportFormat
