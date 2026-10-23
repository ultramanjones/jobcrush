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
// This is not cosmetic. Word and PDF exporters use non-breaking spaces so a
// name does not wrap mid-line. A regular expression does not treat U+00A0 as
// whitespace, so "\s" skips it. The whole resume line then arrives as one
// unsplittable word, every splitter fails, and the school box ends up holding
// "Tulsa Community College 2000 - 2001 Auburn University 1993 - 1994".
//
// Bullets have the same problem. The characters ∙ • · ● all mean "next field".
// The parser should only have to know one of them, so they all become that
// one.
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

    // Dashes the date patterns do not recognize.
    //
    // A resume typed in Word often has a minus sign or a figure dash between
    // two years instead of a hyphen. "2000 - 2001" then fails to match as a
    // range. The parser falls back to a single year and shows 2000 as the year
    // the user finished. A second school on the same line loses its dates
    // entirely, and in the work history a job whose dates will not parse gets
    // dropped with no message.
    //
    // En dash and em dash are deliberately left out of this list. The parser
    // already reads those, so there is no reason to change them.
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
