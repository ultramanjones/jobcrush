#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// PostingRequirementReader
//
// Reads a posting and lists what the employer asked for: a resume, a cover
// letter, a portfolio, references, a transcript, salary expectations. Turns
// that into a checklist.
//
// Runs locally. Costs nothing and needs no AI. This matters: the checklist is
// the one part of a packet a user with no AI key still gets, and this app is
// supposed to be useful without one.
//
// No state, so it stays header-only.
class PostingRequirementReader {
public:
    struct Requirement {
        QString displayText;   // "A cover letter"
        QString evidenceText;  // the words in the posting that asked for it
    };

    QList<Requirement> requirementsIn(const QString &postingText) const
    {
        QList<Requirement> requirementsFound;
        if (postingText.trimmed().isEmpty()) {
            return requirementsFound;
        }

        struct Candidate {
            const char *patternText;
            const char *displayText;
        };

        // In the order a person would gather them, not alphabetically.
        static const Candidate candidates[] = {
            { "\\bcover\\s+letters?\\b",                      "A cover letter" },
            { "\\bresum(?:e|é)s?\\b|\\bcurriculum\\s+vitae\\b|\\bcv\\b", "A resume" },
            { "\\bportfolio\\b|\\bwork\\s+samples?\\b",        "A portfolio or work samples" },
            { "\\bwriting\\s+samples?\\b",                     "A writing sample" },
            { "\\breferences?\\b",                             "References" },
            { "\\btranscripts?\\b",                            "A transcript" },
            { "\\bcertification?s?\\b|\\blicens(?:e|ure)\\b",  "A certification or licence" },
            { "\\bsalary\\s+(?:expectation|requirement)s?\\b|\\bdesired\\s+salary\\b",
              "Your salary expectations" },
            { "\\bavailability\\b|\\bstart\\s+date\\b",        "Your availability or start date" },
            { "\\bwork\\s+authoriz(?:ation|ed)\\b|\\bright\\s+to\\s+work\\b",
              "Confirmation you can work there" },
            { "\\bdriver'?s?\\s+licen[cs]e\\b",                "A driver's licence" },
            { "\\bbackground\\s+check\\b",                     "Consent to a background check" },
            { "\\bcode\\s+(?:test|challenge)\\b|\\btake[-\\s]home\\b",
              "A code test or take-home" },
        };

        for (const Candidate &candidate : candidates) {
            const QRegularExpression askPattern(
                QString::fromLatin1(candidate.patternText),
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match = askPattern.match(postingText);
            if (!match.hasMatch()) {
                continue;
            }
            Requirement requirement;
            requirement.displayText = QString::fromLatin1(candidate.displayText);
            requirement.evidenceText = sentenceAround(postingText, match.capturedStart());
            requirementsFound.append(requirement);
        }
        return requirementsFound;
    }

    // The checklist as markdown, ready to be a piece of a packet.
    //
    // Boxes always start unchecked. The app cannot know what the user has
    // already attached.
    QString checklistMarkdownFor(const QString &postingText) const
    {
        const QList<Requirement> requirementsFound = requirementsIn(postingText);

        QString checklistText = QStringLiteral("## What this employer asked for\n\n");
        if (requirementsFound.isEmpty()) {
            checklistText += QStringLiteral(
                "The posting doesn't spell out what to send. A resume and a cover "
                "letter are the safe answer — add anything else it hints at.\n\n"
                "- [ ] A resume\n"
                "- [ ] A cover letter\n");
            return checklistText;
        }

        for (const Requirement &requirement : requirementsFound) {
            checklistText += QStringLiteral("- [ ] %1\n").arg(requirement.displayText);
        }
        checklistText += QStringLiteral(
            "\nRead from the posting itself. If it asked for something that isn't "
            "here, add it — this list is yours to edit.\n");
        return checklistText;
    }

private:
    // The sentence the match was found in, so the checklist can show where it
    // got each item.
    static QString sentenceAround(const QString &text, int positionInText)
    {
        if (positionInText < 0) {
            return QString();
        }
        int sentenceStart = positionInText;
        while (sentenceStart > 0 && text.at(sentenceStart - 1) != QLatin1Char('.')
               && text.at(sentenceStart - 1) != QLatin1Char('\n')) {
            --sentenceStart;
        }
        int sentenceEnd = positionInText;
        while (sentenceEnd < text.length() && text.at(sentenceEnd) != QLatin1Char('.')
               && text.at(sentenceEnd) != QLatin1Char('\n')) {
            ++sentenceEnd;
        }
        return text.mid(sentenceStart, sentenceEnd - sentenceStart).simplified();
    }
};
