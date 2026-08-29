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
            static const QRegularExpression escapedFirstCharacter(
                QStringLiteral("^\\\\([#>*_\\-+])"));
            const QRegularExpressionMatch escapeMatch = escapedFirstCharacter.match(line);
            if (escapeMatch.hasMatch()) {
                paragraphLines.append(escapeMatch.captured(1) + line.mid(2));
                continue;
            }

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

            static const QRegularExpression rulePattern(QStringLiteral("^(?:-{3,}|\\*{3,}|_{3,})$"));
            if (rulePattern.match(line).hasMatch()) {
                flushParagraph();
                Block rule;
                rule.blockKind = BlockKind::Rule;
                blocks.append(rule);
                continue;
            }

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

    // Splits one line into bold and italic runs. Handles **bold**, *italic*
    // and _italic_. That is all the formatting this app writes.
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
