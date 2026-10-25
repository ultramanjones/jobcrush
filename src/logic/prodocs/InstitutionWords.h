#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// InstitutionWords
//
// The words that identify a school, kept in one place.
//
// Two readers need this list: the resume parser, which finds schools scattered
// through a document, and the transcript reader. Two copies would drift apart,
// and the result would be a school one reader recognizes and the other does
// not.
namespace InstitutionWords {

inline const QStringList &wordsThatNameASchool()
{
    static const QStringList schoolWords = {
        QStringLiteral("university"), QStringLiteral("college"), QStringLiteral("institute"),
        QStringLiteral("academy"), QStringLiteral("school"), QStringLiteral("polytechnic"),
        QStringLiteral("seminary"), QStringLiteral("conservatory"),
        // "Univ Alabama Huntsville" is how it appears on a real transcript;
        // matching only the full word would have missed it entirely.
        QStringLiteral("univ"),
    };
    return schoolWords;
}

// True if the text contains a school word anywhere, even inside a longer
// word. "univ" therefore matches "University" too.
inline bool namesASchool(const QString &text)
{
    const QString loweredText = text.toLower();
    for (const QString &schoolWord : wordsThatNameASchool()) {
        if (loweredText.contains(schoolWord)) {
            return true;
        }
    }
    return false;
}

// True if this single word is a school word, or a school word with an "s"
// on the end ("Colleges"). Strips surrounding punctuation first, since
// transcripts add plenty. Unlike namesASchool, the whole word must match.
inline bool thisWordNamesASchool(const QString &word)
{
    QString bareWord = word.toLower();
    // Anything that is not a lowercase letter, at the start or at the end.
    // "(University," becomes "university".
    static const QRegularExpression edgePunctuationPattern(
        QStringLiteral("^[^a-z]+|[^a-z]+$"));
    bareWord.remove(edgePunctuationPattern);
    if (bareWord.isEmpty()) {
        return false;
    }
    for (const QString &schoolWord : wordsThatNameASchool()) {
        if (bareWord == schoolWord || bareWord == schoolWord + QStringLiteral("s")) {
            return true;
        }
    }
    return false;
}

} // namespace InstitutionWords
