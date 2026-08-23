#pragma once

#include <QString>

// DocumentTextExtractor
//
// Pulls readable words out of a dropped file. Everything above ProDocs works
// on clean text and never touches a file format — AIBrain in particular is
// handed words, never raw bytes, which is the plan's law.
//
// A calculation with no state, so it lives in ModelView as a plain class
// rather than a QObject.
class DocumentTextExtractor {
public:
    // The result of trying. Both fields matter: text is what was read, and
    // note explains an empty result rather than leaving the user to assume
    // their resume loaded when it did not.
    struct ExtractionResult {
        QString extractedText;
        QString note;             // empty when the text came out fine
    };

    // Reads the file AND normalizes what came out — see the .cpp for why the
    // normalizing step is not optional.
    ExtractionResult extractTextFrom(const QString &filePath) const;

private:
    // Picks the right reader for the file. Called only by extractTextFrom,
    // which is what guarantees nothing skips normalization on the way out.
    ExtractionResult extractTextFromFileByKind(const QString &filePath) const;

    ExtractionResult extractFromPlainTextFile(const QString &filePath) const;
    ExtractionResult extractFromPortableDocument(const QString &filePath) const;
    ExtractionResult extractFromWordDocument(const QString &filePath) const;
};
