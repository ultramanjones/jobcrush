#pragma once

#include <QString>
#include <QStringList>

// DocumentKind
//
// What a ProDocs item is. Stored as plain words, same philosophy as the rest
// of the database — a human opening the file can read their own data.
namespace DocumentKind {

inline const QString Resume        = QStringLiteral("resume");
inline const QString CoverLetter   = QStringLiteral("coverLetter");
inline const QString Transcript    = QStringLiteral("transcript");
inline const QString Certification = QStringLiteral("certification");
inline const QString Reference     = QStringLiteral("reference");
inline const QString Photo         = QStringLiteral("photo");
inline const QString Other         = QStringLiteral("other");

// What the user reads.
inline QString displayNameFor(const QString &documentKind)
{
    if (documentKind == Resume)        return QStringLiteral("Resume");
    if (documentKind == CoverLetter)   return QStringLiteral("Cover letter");
    if (documentKind == Transcript)    return QStringLiteral("Transcript");
    if (documentKind == Certification) return QStringLiteral("Certification");
    if (documentKind == Reference)     return QStringLiteral("Reference");
    if (documentKind == Photo)         return QStringLiteral("Photo");
    return QStringLiteral("Document");
}

inline QStringList allDocumentKinds()
{
    return { Resume, CoverLetter, Transcript, Certification, Reference, Photo, Other };
}

} // namespace DocumentKind
