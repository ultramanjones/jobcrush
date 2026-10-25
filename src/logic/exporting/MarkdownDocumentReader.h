#pragma once

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

// MarkdownDocumentReader
//
// Reads the app's internal markdown into blocks that can be turned into a Word
// or PDF document.
//
// This is not a full markdown implementation and is not meant to be. It reads
// only what Job Crush writes: headings, paragraphs, bullets, checkboxes,
// horizontal rules, and bold or italic within a line. That covers a cover
// letter and a resume.
//
// No state, so it stays header-only.
class MarkdownDocumentReader {
public:
    enum class BlockKind { Heading, Paragraph, Bullet, CheckboxUnticked, CheckboxTicked, Rule };

    // One run of text with its formatting. A paragraph is a list of these, so
    // "**Acme** - 2019" keeps its bold when it becomes a Word run.
    struct TextRun {
        QString text;
        bool isBold = false;
        bool isItalic = false;
    };

    struct Block {
        BlockKind blockKind = BlockKind::Paragraph;
        int headingLevel = 0;        // 1-6, Heading only
        QList<TextRun> runs;

        QString plainText() const
        {
            QString joined;
            for (const TextRun &run : runs) {
                joined += run.text;
            }
            return joined;
        }
    };

    // Splits markdown into blocks, one line at a time.
    //
    // Each line is trimmed, then tested in this order. The first test that
    // matches decides what the line is:
    //   1. empty                 ends the paragraph being collected
    //   2. starts with "\#" etc. text, with the backslash removed
    //   3. "# Title" to "###### Title"   a heading
    //   4. "---", "***" or "___"         a rule
    //   5. "- [ ] text", "- [x] text"    a checkbox
    //   6. "- text", "* text", "• text"  a bullet
    //   7. anything else                 one more line of the current paragraph
    //
    // Lines of a paragraph are collected until a blank line or another kind
    // of block, then joined with spaces into one Paragraph block. Order
    // matters: the checkbox test comes before the bullet test, because every
    // checkbox line would also pass as a bullet.
    QList<Block> blocksIn(const QString &markdownText) const
    {
        QList<Block> blocks;

        const QStringList lines = markdownText.split(QLatin1Char('\n'));
        QStringList paragraphLines;

        auto flushParagraph = [&]() {
            if (paragraphLines.isEmpty()) {
                return;
            }
            Block paragraph;
            paragraph.blockKind = BlockKind::Paragraph;
            paragraph.runs = runsIn(paragraphLines.join(QLatin1Char(' ')));
            blocks.append(paragraph);
            paragraphLines.clear();
        };

        for (const QString &rawLine : lines) {
            const QString line = rawLine.trimmed();

            if (line.isEmpty()) {
                flushParagraph();
                continue;
            }

            // A backslash in front of a character that would otherwise be an
            // instruction means the writer wanted the character itself. A
            // resume line reading "#1 salesperson" is not a heading, and the
            // backslash that protects it must not survive into the document.
            // At the start: a backslash, then one of # > * _ - +.
            // captured(1) is that character, kept without the backslash.
            static const QRegularExpression escapedFirstCharacter(
                QStringLiteral("^\\\\([#>*_\\-+])"));
            const QRegularExpressionMatch escapeMatch = escapedFirstCharacter.match(line);
            if (escapeMatch.hasMatch()) {
                paragraphLines.append(escapeMatch.captured(1) + line.mid(2));
                continue;
            }

            // One to six "#", at least one space, then the heading text.
            // captured(1) is the "#" marks (their count is the level), and
            // captured(2) is the text.
            static const QRegularExpression headingPattern(QStringLiteral("^(#{1,6})\\s+(.*)$"));
            const QRegularExpressionMatch headingMatch = headingPattern.match(line);
            if (headingMatch.hasMatch()) {
                flushParagraph();
                Block heading;
                heading.blockKind = BlockKind::Heading;
                heading.headingLevel = headingMatch.captured(1).length();
                heading.runs = runsIn(headingMatch.captured(2));
                blocks.append(heading);
                continue;
            }

            // Three or more of the same mark ("-", "*" or "_") and nothing else.
            static const QRegularExpression rulePattern(QStringLiteral("^(?:-{3,}|\\*{3,}|_{3,})$"));
            if (rulePattern.match(line).hasMatch()) {
                flushParagraph();
                Block rule;
                rule.blockKind = BlockKind::Rule;
                blocks.append(rule);
                continue;
            }

            // A list mark ("-", "*" or "+"), spaces, a box "[ ]", "[x]" or
            // "[X]", spaces, then the text. captured(1) is what is inside the
            // box, and captured(2) is the text.
            static const QRegularExpression checkboxPattern(
                QStringLiteral("^[-*+]\\s+\\[([ xX])\\]\\s+(.*)$"));
            const QRegularExpressionMatch checkboxMatch = checkboxPattern.match(line);
            if (checkboxMatch.hasMatch()) {
                flushParagraph();
                Block checkbox;
                checkbox.blockKind = checkboxMatch.captured(1).trimmed().isEmpty()
                    ? BlockKind::CheckboxUnticked
                    : BlockKind::CheckboxTicked;
                checkbox.runs = runsIn(checkboxMatch.captured(2));
                blocks.append(checkbox);
                continue;
            }

            // A list mark ("-", "*", "+" or "•"), spaces, then the text.
            static const QRegularExpression bulletPattern(QStringLiteral("^[-*+•]\\s+(.*)$"));
            const QRegularExpressionMatch bulletMatch = bulletPattern.match(line);
            if (bulletMatch.hasMatch()) {
                flushParagraph();
                Block bullet;
                bullet.blockKind = BlockKind::Bullet;
                bullet.runs = runsIn(bulletMatch.captured(1));
                blocks.append(bullet);
                continue;
            }

            paragraphLines.append(line);
        }
        flushParagraph();

        return blocks;
    }

    // Splits one line into bold and italic runs. Handles **bold**, __bold__,
    // *italic* and _italic_. That is all the formatting this app writes.
    //
    // How it works:
    //   1. Find each marked stretch, left to right. The pattern has four
    //      alternatives, one per mark. The two-character marks come first,
    //      so "**word**" is read as bold and not as italic "*word*" inside
    //      stray stars. ".+?" takes as few characters as it can, so the
    //      stretch ends at the FIRST closing mark.
    //   2. Text between the last match and this one becomes a plain run.
    //   3. The match becomes a bold run (alternative 1 or 2) or an italic
    //      run (alternative 3 or 4). The marks themselves are dropped.
    //   4. Text after the last match becomes a final plain run.
    //   5. A line with no text at all still gets one empty run.
    //
    // Two underscores on one line, as in "first_last" and "snake_case",
    // turn the text between them italic and the underscores disappear. That
    // can happen to an email address or a file name in a resume.
    QList<TextRun> runsIn(const QString &lineText) const
    {
        QList<TextRun> runs;

        static const QRegularExpression emphasisPattern(
            QStringLiteral("\\*\\*(.+?)\\*\\*|__(.+?)__|\\*(.+?)\\*|_(.+?)_"));

        int positionInLine = 0;
        QRegularExpressionMatchIterator matches = emphasisPattern.globalMatch(lineText);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();

            if (match.capturedStart() > positionInLine) {
                TextRun plainRun;
                plainRun.text = lineText.mid(positionInLine,
                                             match.capturedStart() - positionInLine);
                runs.append(plainRun);
            }

            TextRun emphasisedRun;
            if (match.captured(1).length() > 0 || match.captured(2).length() > 0) {
                emphasisedRun.isBold = true;
                emphasisedRun.text = match.captured(1).isEmpty() ? match.captured(2)
                                                                 : match.captured(1);
            } else {
                emphasisedRun.isItalic = true;
                emphasisedRun.text = match.captured(3).isEmpty() ? match.captured(4)
                                                                 : match.captured(3);
            }
            runs.append(emphasisedRun);
            positionInLine = match.capturedEnd();
        }

        if (positionInLine < lineText.length()) {
            TextRun trailingRun;
            trailingRun.text = lineText.mid(positionInLine);
            runs.append(trailingRun);
        }

        if (runs.isEmpty()) {
            TextRun emptyRun;
            emptyRun.text = lineText;
            runs.append(emptyRun);
        }
        return runs;
    }
};
