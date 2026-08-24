#include "TaskReplyReader.h"

#include <QRegularExpression>
#include <QStringList>

int TaskReplyReader::fitScoreIn(const QString &replyText)
{
    // The format we asked for, first.
    static const QRegularExpression declaredFitPattern(
        QStringLiteral("^\\s*FIT\\s*[:\\-]?\\s*(\\d{1,3})\\s*%?"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    const QRegularExpressionMatch declaredMatch = declaredFitPattern.match(replyText);
    if (declaredMatch.hasMatch()) {
        const int score = declaredMatch.captured(1).toInt();
        if (score >= 0 && score <= 100) {
            return score;
        }
    }

    // Then the formats models use when they forget: "72/100" and "72%".
    static const QRegularExpression outOfHundredPattern(
        QStringLiteral("\\b(\\d{1,3})\\s*/\\s*100\\b"));
    const QRegularExpressionMatch outOfHundredMatch = outOfHundredPattern.match(replyText);
    if (outOfHundredMatch.hasMatch()) {
        const int score = outOfHundredMatch.captured(1).toInt();
        if (score >= 0 && score <= 100) {
            return score;
        }
    }

    static const QRegularExpression percentagePattern(QStringLiteral("\\b(\\d{1,3})\\s*%"));
    const QRegularExpressionMatch percentageMatch = percentagePattern.match(replyText);
    if (percentageMatch.hasMatch()) {
        const int score = percentageMatch.captured(1).toInt();
        if (score >= 0 && score <= 100) {
            return score;
        }
    }

    // No number found. Report that instead of making one up.
    return -1;
}

QString TaskReplyReader::withoutTheChatter(const QString &replyText)
{
    QString tidyText = replyText.trimmed();

    // The whole answer wrapped in one code fence.
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
    static const QRegularExpression openingChatterPattern(
        QStringLiteral("^(?:sure|certainly|of course|here(?:'s| is)|absolutely)\\b[^\\n]{0,80}:\\s*\\n+"),
        QRegularExpression::CaseInsensitiveOption);
    tidyText.remove(openingChatterPattern);

    // A closing offer of more help.
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
        // read, remove the line so the user does not see it.
        static const QRegularExpression declaredFitLinePattern(
            QStringLiteral("^\\s*FIT\\s*[:\\-]?\\s*\\d{1,3}\\s*%?\\s*\\n+"),
            QRegularExpression::CaseInsensitiveOption);
        outcome.markdownText.remove(declaredFitLinePattern);
        outcome.markdownText = outcome.markdownText.trimmed();
    }

    return outcome;
}
