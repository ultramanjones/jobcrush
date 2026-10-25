#include "DroppedFileClassifier.h"

#include <QFileInfo>

#include "DocumentKind.h"
#include "DroppedFileTypes.h"

namespace {

// How many times a body word has to appear before it scores. Set to 1, so
// one mention is enough. One passing mention cannot decide the kind on its
// own, though: a body word scores pointsForABodyWord, and the bar below
// (lowestScoreToTrustTheGuess) needs two different body words, or a
// filename word.
constexpr int occurrencesThatCount = 1;

// A word in the filename. The user named the file, which is close to
// telling us outright.
constexpr int pointsForAFileNameWord = 10;

// A phrase found in the document's text.
constexpr int pointsForABodyWord = 2;

// Below this, the guess is "other". Two body words agreeing is thin
// evidence, but it is enough.
constexpr int lowestScoreToTrustTheGuess = 4;

// Counts how many times loweredTerm appears in loweredText, without
// overlaps. Each search starts right after the end of the last match.

int countOccurrences(const QString &loweredText, const QString &loweredTerm)
{
    int occurrences = 0;
    int searchPosition = loweredText.indexOf(loweredTerm);
    while (searchPosition >= 0) {
        ++occurrences;
        searchPosition = loweredText.indexOf(loweredTerm, searchPosition + loweredTerm.length());
    }
    return occurrences;
}

// Signals for one kind: words in the FILENAME are worth more than words in
// the body, because a person who named a file "resume-2026.pdf" has told us
// outright and should not be second-guessed by a paragraph.
struct KindSignals {
    QString documentKind;
    QStringList fileNameWords;
    QStringList bodyWords;
};

QList<KindSignals> allKindSignals()
{
    return {
        { DocumentKind::Resume,
          { QStringLiteral("resume"), QStringLiteral("cv"),
            QStringLiteral("curriculum") },
          { QStringLiteral("work experience"), QStringLiteral("professional experience"),
            QStringLiteral("employment history"), QStringLiteral("education"),
            QStringLiteral("skills") } },

        { DocumentKind::CoverLetter,
          { QStringLiteral("cover"), QStringLiteral("letter") },
          { QStringLiteral("dear hiring"), QStringLiteral("dear sir"),
            QStringLiteral("i am writing"), QStringLiteral("sincerely"),
            QStringLiteral("i would welcome") } },

        { DocumentKind::Transcript,
          { QStringLiteral("transcript") },
          { QStringLiteral("grade point"), QStringLiteral("gpa"),
            QStringLiteral("credit hours"), QStringLiteral("semester"),
            QStringLiteral("cumulative") } },

        { DocumentKind::Certification,
          { QStringLiteral("certificate"), QStringLiteral("certification"),
            QStringLiteral("license"), QStringLiteral("diploma") },
          { QStringLiteral("is hereby certified"), QStringLiteral("has completed"),
            QStringLiteral("certificate of"), QStringLiteral("awarded to") } },

        { DocumentKind::Reference,
          { QStringLiteral("reference"), QStringLiteral("recommendation") },
          { QStringLiteral("letter of recommendation"),
            QStringLiteral("it is my pleasure to recommend"),
            QStringLiteral("i highly recommend"), QStringLiteral("worked with") } },
    };
}

} // namespace

// Scores every kind and keeps the highest:
//   1. An image is a photo. Nothing else is checked.
//   2. For each kind, add points for each of its words found in the
//      filename, and fewer points for each of its phrases found in the text.
//   3. The kind with the most points wins. On a tie, the kind listed first
//      in allKindSignals wins, because only a higher score replaces it.
//   4. If even the winner is below the bar, the answer is "other".
QString DroppedFileClassifier::classifyDocument(const QString &filePath,
                                                const QString &documentText) const
{
    if (DroppedFileTypes::isImage(filePath)) {
        return DocumentKind::Photo;
    }

    const QString loweredFileName = QFileInfo(filePath).completeBaseName().toLower();
    const QString loweredBodyText = documentText.toLower();

    QString bestKind = DocumentKind::Other;
    int bestScore = 0;

    for (const KindSignals &kindSignals : allKindSignals()) {
        int score = 0;

        for (const QString &fileNameWord : kindSignals.fileNameWords) {
            if (loweredFileName.contains(fileNameWord)) {
                score += pointsForAFileNameWord;
            }
        }
        for (const QString &bodyWord : kindSignals.bodyWords) {
            if (countOccurrences(loweredBodyText, bodyWord) >= occurrencesThatCount) {
                score += pointsForABodyWord;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestKind = kindSignals.documentKind;
        }
    }

    // Below the bar, admit to not knowing rather than filing it somewhere
    // confident and wrong — "other" is honest, and the user can put it right
    // in one click.
    return bestScore >= lowestScoreToTrustTheGuess ? bestKind : DocumentKind::Other;
}
