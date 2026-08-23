#pragma once

#include <QChar>
#include <QLatin1Char>
#include <QString>

// PlainTextNormalizer
//
// One alphabet, for everything that reads a person's documents.
//
// Shared by DocumentTextExtractor (so text is normalized on the way IN) and
// ResumeInsightParser (so text already stored from an older build is
// normalized on the way to the parser). Both, deliberately: normalizing only
// at extraction leaves every document imported before this existed carrying
// characters the parser cannot handle, and their owner has no idea why their
// schooling looks wrong.

// Puts every document into ONE alphabet before anything tries to read it.
//
// This is not tidiness. Word and every PDF exporter lay resumes out with
// NON-BREAKING spaces so a name never wraps mid-line, and a non-breaking
// space is not a space as far as a regular expression is concerned: "\s"
// walks straight past U+00A0. A whole resume line therefore arrives as ONE
// unsplittable word, every splitter in the parser gives up, and the school
// box ends up holding "Tulsa Community College 2000 - 2001 Auburn University
// 1993 - 1994" — which is exactly the bug this function exists to end.
//
// Same story for the bullet a designer picked: ∙ • · ● and friends all mean
// "next field", and the parser should not have to know six of them. They all
// become the one the parser already understands.
//
// Doing this HERE means every reader downstream — the resume parser today,
// whatever reads a cover letter tomorrow — is handed plain, ordinary text.
inline QString withEveryLookalikeCharacterNormalized(const QString &rawText)
{
    QString normalizedText = rawText;

    // Spaces that are not the space character.
    static const QString spaceLookalikes = QStringLiteral(
        "\u00A0\u1680\u2000\u2001\u2002\u2003\u2004\u2005\u2006"
        "\u2007\u2008\u2009\u200A\u202F\u205F\u3000");
    for (const QChar spaceLookalike : spaceLookalikes) {
        normalizedText.replace(spaceLookalike, QLatin1Char(' '));
    }

    // Bullets and dot separators that all mean the same thing.
    static const QString bulletLookalikes = QStringLiteral(
        "\u2022\u2023\u2043\u2219\u22C5\u25AA\u25CF\u25E6\u2027\u30FB");
    for (const QChar bulletLookalike : bulletLookalikes) {
        normalizedText.replace(bulletLookalike, QChar(0x00B7)); // ·
    }

    // Dashes that are not the dash the date patterns know.
    //
    // This is the other half of the same bug, and the half that survives
    // fixing the spaces. A resume typed in Word carries a MINUS SIGN or a
    // figure dash between two years, not a hyphen — so "2000 - 2001" never
    // matches as a range, the parser falls back to picking up a lone year,
    // and the user is shown 2000 as the year they FINISHED a course they
    // started in 2000. A second school listed on the same line loses its
    // years altogether, and in the work history an entire job whose dates
    // will not parse is dropped without a word.
    //
    // The en dash and em dash are deliberately NOT in this list: the parser
    // already reads those, and rewriting punctuation it understands would be
    // this app editing somebody's resume for no reason.
    static const QString dashLookalikes = QStringLiteral(
        "\u2010\u2011\u2012\u2015\u2212\uFE58\uFE63\uFF0D");
    for (const QChar dashLookalike : dashLookalikes) {
        normalizedText.replace(dashLookalike, QLatin1Char('-'));
    }

    // Present in the file, invisible on the page, and pure noise to a parser.
    static const QString charactersWithNoWidth = QStringLiteral(
        "\u200B\u200C\u200D\uFEFF\u00AD");
    for (const QChar noWidthCharacter : charactersWithNoWidth) {
        normalizedText.remove(noWidthCharacter);
    }

    // Line separators that are not newlines. A resume laid out in a table can
    // arrive full of these, and every entry after the first goes missing.
    normalizedText.replace(QChar(0x2028), QLatin1Char('\n'));
    normalizedText.replace(QChar(0x2029), QLatin1Char('\n'));
    normalizedText.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalizedText.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    return normalizedText;
}
