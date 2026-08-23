#include "DroppedFileClassifier.h"

#include <QFileInfo>

#include "DocumentKind.h"
#include "DroppedFileTypes.h"

namespace {

// How many times a word has to appear before it means anything. Resumes say
// "experience" and "education" as headings; a cover letter might mention
// either once in passing, and one mention should not tip the scales.
constexpr int occurrencesThatCount = 1;

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
                score += 10; // the user named it; that is close to a declaration
            }
        }
        for (const QString &bodyWord : kindSignals.bodyWords) {
            if (countOccurrences(loweredBodyText, bodyWord) >= occurrencesThatCount) {
                score += 2;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestKind = kindSignals.documentKind;
        }
    }

    // Two body words agreeing is thin evidence. Below that, admit to not
    // knowing rather than filing it somewhere confident and wrong — "other"
    // is honest, and the user can put it right in one click.
    return bestScore >= 4 ? bestKind : DocumentKind::Other;
}
