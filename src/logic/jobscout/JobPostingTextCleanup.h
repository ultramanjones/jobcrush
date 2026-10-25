#pragma once

#include <QRegularExpression>
#include <QString>

// JobPostingTextCleanup
//
// Job boards hand back descriptions as HTML fragments. Everything above the
// source clients wants plain readable words: the scorer counts them, the
// Discoveries row shows a line of them, and a future AIBrain task must never
// be fed raw markup (that is the plan's law — AIBrain receives clean text,
// never raw anything).
//
// Deliberately small and shared, so every source cleans up identically. This
// is not a general HTML parser and does not pretend to be one — job-board
// descriptions are simple markup, and the goal is readable words, not a
// faithful document tree.

// Decodes the character entities that actually turn up in job descriptions,
// plus the numeric form. Deliberately not the full HTML entity table: an
// unknown entity is left alone rather than guessed at, because "&pound;60k"
// half-decoded into nonsense is worse than "&pound;60k" left intact.
inline void decodeOneLayerOfCharacterEntities(QString &workingText)
{
    static const QList<QPair<QString, QString>> namedEntities = {
        { QStringLiteral("&nbsp;"),   QStringLiteral(" ")  },
        { QStringLiteral("&lt;"),     QStringLiteral("<")  },
        { QStringLiteral("&gt;"),     QStringLiteral(">")  },
        { QStringLiteral("&quot;"),   QStringLiteral("\"") },
        { QStringLiteral("&apos;"),   QStringLiteral("'")  },
        { QStringLiteral("&#39;"),    QStringLiteral("'")  },
        { QStringLiteral("&rsquo;"),  QStringLiteral("'")  },
        { QStringLiteral("&lsquo;"),  QStringLiteral("'")  },
        { QStringLiteral("&ldquo;"),  QStringLiteral("“") },
        { QStringLiteral("&rdquo;"),  QStringLiteral("”") },
        { QStringLiteral("&ndash;"),  QStringLiteral("–") },
        { QStringLiteral("&mdash;"),  QStringLiteral("—") },
        { QStringLiteral("&hellip;"), QStringLiteral("…") },
        { QStringLiteral("&bull;"),   QStringLiteral("•") },
        { QStringLiteral("&middot;"), QStringLiteral("·") },
        { QStringLiteral("&pound;"),  QStringLiteral("£") },
        { QStringLiteral("&euro;"),   QStringLiteral("€") },
        { QStringLiteral("&trade;"),  QStringLiteral("™") },
        { QStringLiteral("&reg;"),    QStringLiteral("®") },
        // Ampersand LAST. Decoding it first would turn "&amp;lt;" into
        // "&lt;" and then into "<" in the same pass, which is precisely the
        // double-escaping trick this whole file exists to survive.
        { QStringLiteral("&amp;"),    QStringLiteral("&")  },
    };
    for (const auto &entity : namedEntities) {
        workingText.replace(entity.first, entity.second);
    }

    // The numeric forms: &#8212; and &#x2014;
    //
    // The pattern is "&#", then an optional "x" (captured as group 1), then
    // one to six hex digits (group 2), then ";". With no "x" the digits are
    // read as decimal, and with an "x" they are read as hex.
    //
    // The text is rebuilt from left to right:
    //  1. For each match, read the digits as a number.
    //  2. If they do not parse, or the number is 0, or it is past the last
    //     Unicode character (0x10FFFF), skip this match. The entity is left
    //     in the text as it was, because the next copy picks it up.
    //  3. Otherwise copy the plain text between the last match and this
    //     one, then add the one character the number names.
    //  4. After the last match, copy whatever text is left.
    static const QRegularExpression numericEntityPattern(
        QStringLiteral("&#(x?)([0-9a-fA-F]{1,6});"));
    QString decodedText;
    decodedText.reserve(workingText.size());
    int copiedUpTo = 0;
    QRegularExpressionMatchIterator matches = numericEntityPattern.globalMatch(workingText);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        bool digitsParsed = false;
        const uint codePoint = match.captured(2).toUInt(
            &digitsParsed, match.captured(1).isEmpty() ? 10 : 16);
        if (!digitsParsed || codePoint == 0 || codePoint > 0x10FFFF) {
            continue;
        }
        decodedText += workingText.mid(copiedUpTo, match.capturedStart() - copiedUpTo);
        decodedText += QString::fromUcs4(reinterpret_cast<const char32_t *>(&codePoint), 1);
        copiedUpTo = match.capturedEnd();
    }
    decodedText += workingText.mid(copiedUpTo);
    workingText = decodedText;
}

// The most strip-and-decode passes plainTextFromHtmlFragment makes. Real
// postings are escaped at most twice. The limit stops a page built to be
// escaped over and over from keeping the loop going forever.
constexpr int mostHtmlCleanupPasses = 3;

// Turns an HTML description fragment into plain readable text.
inline QString plainTextFromHtmlFragment(const QString &htmlFragment)
{
    QString workingText = htmlFragment;

    // Block-level tags become line breaks BEFORE tags are stripped, so
    // paragraphs and list items don't run together into one wall of words.
    //
    // The pattern is "<", optional spaces, an optional "/", optional spaces,
    // then one of p, br, div, li, tr or h1 to h6 as a whole word, then
    // anything up to the closing ">". So "<p>", "</li>", "<br/>" and
    // "<div class=x>" all match.
    static const QRegularExpression blockBreakTagPattern(
        QStringLiteral("<\\s*/?\\s*(p|br|div|li|tr|h[1-6])\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);

    // Tag-SHAPED, not merely bracketed. A blanket "<...>" sweep is tempting
    // and wrong: it eats "we get 5 > 3 candidates <shrug>" down to nothing,
    // and a job description is allowed to contain a less-than sign.
    //
    // The pattern is "<", optional spaces, an optional "/", optional spaces,
    // then a tag name (a letter, then letters, digits, ":" or "-"), then
    // optionally a space followed by attributes with no "<" or ">" in them,
    // then an optional "/", optional spaces, and ">". "5 > 3" has no tag
    // name after a "<", so it is left alone.
    static const QRegularExpression htmlTagPattern(
        QStringLiteral("<\\s*/?\\s*[a-zA-Z][a-zA-Z0-9:-]*(?:\\s[^<>]*)?/?\\s*>"));
    // An HTML comment: "<!--", then the fewest characters possible, across
    // line breaks, then "-->".
    static const QRegularExpression htmlCommentPattern(
        QStringLiteral("<!--.*?-->"), QRegularExpression::DotMatchesEverythingOption);

    // Strip, decode, and strip again.
    //
    // Some boards escape their HTML once and some escape it twice. Decoding
    // "&lt;img src=…&gt;" AFTER stripping tags puts a live <img> back into
    // text we had finished cleaning, and a Text element in Qt's default
    // AutoText mode will happily go and fetch it. That is exactly how a
    // recruiter's banner ended up rendered full-size inside a Discoveries row.
    //
    // So each pass strips what it can see, then decodes one layer of escaping,
    // and the loop goes round again until a pass changes nothing, or until
    // mostHtmlCleanupPasses passes have run.
    for (int cleanupPass = 0; cleanupPass < mostHtmlCleanupPasses; ++cleanupPass) {
        const QString textBeforeThisPass = workingText;

        workingText.remove(htmlCommentPattern);
        workingText.replace(blockBreakTagPattern, QStringLiteral("\n"));
        workingText.remove(htmlTagPattern);
        decodeOneLayerOfCharacterEntities(workingText);

        if (workingText == textBeforeThisPass) {
            break;
        }
    }

    // Whatever the last decode revealed does not get a free pass upward.
    workingText.remove(htmlCommentPattern);
    workingText.remove(htmlTagPattern);

    // Collapse the runs of blank space that stripping always leaves behind.
    // Runs of spaces and tabs become one space. Any run of white space that
    // holds two or more line breaks becomes exactly one blank line.
    static const QRegularExpression runOfSpacesPattern(QStringLiteral("[ \\t]+"));
    workingText.replace(runOfSpacesPattern, QStringLiteral(" "));
    static const QRegularExpression runOfBlankLinesPattern(QStringLiteral("\\s*\\n\\s*\\n\\s*"));
    workingText.replace(runOfBlankLinesPattern, QStringLiteral("\n\n"));

    return workingText.trimmed();
}

// When oneLineSummaryFrom cuts a summary at a space, the space must be past
// this many characters. If the only space is earlier, the cut would throw
// away most of the line, so the text is cut mid-word at the limit instead.
constexpr int shortestSummaryCutAtASpace = 40;

// The one line a Discoveries row shows before the user opens anything. All
// white space becomes single spaces, and text longer than maximumCharacters
// is cut and ends with "…".
inline QString oneLineSummaryFrom(const QString &plainDescriptionText,
                                  int maximumCharacters = 160)
{
    QString singleLineText = plainDescriptionText;
    static const QRegularExpression anyWhitespaceRunPattern(QStringLiteral("\\s+"));
    singleLineText.replace(anyWhitespaceRunPattern, QStringLiteral(" "));
    singleLineText = singleLineText.trimmed();

    if (singleLineText.length() <= maximumCharacters) {
        return singleLineText;
    }

    // Cut on a word boundary rather than mid-word — a summary that ends in
    // "engine" when the word was "engineering" reads like a bug.
    const int lastSpaceBeforeLimit =
        singleLineText.left(maximumCharacters).lastIndexOf(QLatin1Char(' '));
    const int cutPosition = lastSpaceBeforeLimit > shortestSummaryCutAtASpace
                                ? lastSpaceBeforeLimit
                                                      : maximumCharacters;
    return singleLineText.left(cutPosition).trimmed() + QStringLiteral("…");
}
