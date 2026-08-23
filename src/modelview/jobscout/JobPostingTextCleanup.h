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

// Turns an HTML description fragment into plain readable text.
inline QString plainTextFromHtmlFragment(const QString &htmlFragment)
{
    QString workingText = htmlFragment;

    // Block-level tags become line breaks BEFORE tags are stripped, so
    // paragraphs and list items don't run together into one wall of words.
    static const QRegularExpression blockBreakTagPattern(
        QStringLiteral("<\\s*/?\\s*(p|br|div|li|tr|h[1-6])\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    workingText.replace(blockBreakTagPattern, QStringLiteral("\n"));

    // Everything else in angle brackets goes.
    static const QRegularExpression anyRemainingTagPattern(QStringLiteral("<[^>]*>"));
    workingText.remove(anyRemainingTagPattern);

    // The handful of entities that actually show up in job descriptions.
    workingText.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    workingText.replace(QStringLiteral("&amp;"),  QStringLiteral("&"));
    workingText.replace(QStringLiteral("&lt;"),   QStringLiteral("<"));
    workingText.replace(QStringLiteral("&gt;"),   QStringLiteral(">"));
    workingText.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    workingText.replace(QStringLiteral("&#39;"),  QStringLiteral("'"));
    workingText.replace(QStringLiteral("&rsquo;"), QStringLiteral("'"));

    // Collapse the runs of blank space that stripping always leaves behind.
    static const QRegularExpression runOfSpacesPattern(QStringLiteral("[ \\t]+"));
    workingText.replace(runOfSpacesPattern, QStringLiteral(" "));
    static const QRegularExpression runOfBlankLinesPattern(QStringLiteral("\\s*\\n\\s*\\n\\s*"));
    workingText.replace(runOfBlankLinesPattern, QStringLiteral("\n\n"));

    return workingText.trimmed();
}

// The one line a Discoveries row shows before the user opens anything.
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
    const int cutPosition = lastSpaceBeforeLimit > 40 ? lastSpaceBeforeLimit
                                                      : maximumCharacters;
    return singleLineText.left(cutPosition).trimmed() + QStringLiteral("…");
}
