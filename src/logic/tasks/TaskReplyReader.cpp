#include "TaskReplyReader.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

// A fit score is a percent, so it runs from 0 to 100. A number outside that
// is something else in the reply, such as a year or a salary.
constexpr int lowestFitScore = 0;
constexpr int highestFitScore = 100;

bool isAFitScore(int number)
{
    return number >= lowestFitScore && number <= highestFitScore;
}

} // namespace

// Tries three ways of writing the score, most trusted first, and takes the
// first one that gives a number from 0 to 100.
int TaskReplyReader::fitScoreIn(const QString &replyText)
{
    // The format we asked for, first. The pattern, on any line: any spaces,
    // "FIT", any spaces, a colon or dash if there is one, any spaces, then
    // capture 1, one to three digits, then a % sign if there is one.
    static const QRegularExpression declaredFitPattern(
        QStringLiteral("^\\s*FIT\\s*[:\\-]?\\s*(\\d{1,3})\\s*%?"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    const QRegularExpressionMatch declaredMatch = declaredFitPattern.match(replyText);
    if (declaredMatch.hasMatch()) {
        const int score = declaredMatch.captured(1).toInt();
        if (isAFitScore(score)) {
            return score;
        }
    }

    // Then the formats models use when they forget: "72/100" and "72%".
    // "72/100" is one to three digits as a whole word, any spaces, a slash,
    // any spaces, and 100.
    static const QRegularExpression outOfHundredPattern(
        QStringLiteral("\\b(\\d{1,3})\\s*/\\s*100\\b"));
    const QRegularExpressionMatch outOfHundredMatch = outOfHundredPattern.match(replyText);
    if (outOfHundredMatch.hasMatch()) {
        const int score = outOfHundredMatch.captured(1).toInt();
        if (isAFitScore(score)) {
            return score;
        }
    }

    // "72%" is one to three digits as a whole word, any spaces, and a % sign.
    // Only the first percent in the reply is tried.
    static const QRegularExpression percentagePattern(QStringLiteral("\\b(\\d{1,3})\\s*%"));
    const QRegularExpressionMatch percentageMatch = percentagePattern.match(replyText);
    if (percentageMatch.hasMatch()) {
        const int score = percentageMatch.captured(1).toInt();
        if (isAFitScore(score)) {
            return score;
        }
    }

    // No number found. Report that instead of making one up.
    return -1;
}

QString TaskReplyReader::withoutTheChatter(const QString &replyText)
{
    QString tidyText = replyText.trimmed();

    // The whole answer wrapped in one code fence. The pattern: ``` at the very
    // start, an optional language word such as "markdown", the end of that
    // line, then capture 1, everything up to a ``` that is the very end of the
    // text. A fence that only wraps part of the answer does not match.
    static const QRegularExpression wholeAnswerFencedPattern(
        QStringLiteral("^```[a-zA-Z]*\\s*\\n([\\s\\S]*)\\n```$"));
    const QRegularExpressionMatch fencedMatch = wholeAnswerFencedPattern.match(tidyText);
    if (fencedMatch.hasMatch()) {
        tidyText = fencedMatch.captured(1).trimmed();
    }

    // A first line that talks to the reader instead of being part of the
    // document. Only one line, only if it is short and ends in a colon.
    // Anything looser risks deleting the user's first paragraph, which is
    // worse than leaving one stray sentence.
    //
    // The pattern: at the very start, one of "sure", "certainly", "of course",
    // "here's", "here is" or "absolutely" as a whole word. Then up to 80 more
    // characters on the same line, a colon, and the line breaks after it.
    static const QRegularExpression openingChatterPattern(
        QStringLiteral("^(?:sure|certainly|of course|here(?:'s| is)|absolutely)\\b[^\\n]{0,80}:\\s*\\n+"),
        QRegularExpression::CaseInsensitiveOption);
    tidyText.remove(openingChatterPattern);

    // A closing offer of more help. The pattern: one or more line breaks,
    // then a last line that starts with "let me know", "would you like",
    // "i hope this" or "feel free". The $ means it must be the last line.
    static const QRegularExpression closingChatterPattern(
        QStringLiteral("\\n+(?:let me know|would you like|i hope this|feel free)\\b[^\\n]*$"),
        QRegularExpression::CaseInsensitiveOption);
    tidyText.remove(closingChatterPattern);

    return tidyText.trimmed();
}

TaskOutcome TaskReplyReader::read(AiBrainTaskKind taskKind, const QString &replyText) const
{
    TaskOutcome outcome;

    const QString tidyText = withoutTheChatter(replyText);
    if (tidyText.isEmpty()) {
        outcome.succeeded = false;
        outcome.reasonText = QStringLiteral("The brain answered, but the answer was empty.");
        outcome.whatToDoNextText =
            QStringLiteral("Try again — and if it keeps coming back empty, a different "
                           "brain in Settings will usually get past it.");
        return outcome;
    }

    outcome.succeeded = true;
    outcome.markdownText = tidyText;

    if (taskKind == AiBrainTaskKind::ScoreFit) {
        outcome.fitScorePercent = fitScoreIn(tidyText);

        // The FIT: line only existed to carry the number. Once the number is
        // read, remove the line so the user does not see it. The pattern is
        // the same as the FIT pattern in fitScoreIn, plus the line breaks
        // after it. With no multiline option, ^ means the start of the reply,
        // so only a FIT line at the very top is removed.
        static const QRegularExpression declaredFitLinePattern(
            QStringLiteral("^\\s*FIT\\s*[:\\-]?\\s*\\d{1,3}\\s*%?\\s*\\n+"),
            QRegularExpression::CaseInsensitiveOption);
        outcome.markdownText.remove(declaredFitLinePattern);
        outcome.markdownText = outcome.markdownText.trimmed();
    }

    return outcome;
}
