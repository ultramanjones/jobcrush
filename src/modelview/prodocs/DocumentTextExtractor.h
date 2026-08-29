#pragma once

#include <QString>

// DocumentTextExtractor
//
// Pulls readable words out of a dropped file. Everything above ProDocs works
// on clean text and never touches a file format — AIBrain in particular is
// handed words, never raw bytes, which is the plan's law.
//
// One exception, added 2026-08-28: a scanned page holds a picture and no
// words, so there is nothing to extract and local reading has already failed.
// The user may then send that image to the brain to be read. Only on their
// button press, only after being told it spends their AI credits, and never
// as part of anything automatic. Everywhere else the law stands.
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

        // How much of a PDF turned out to be photographs of paper. Counted
        // per page, because a document can be half one and half the other.
        // Both stay 0 for every other kind of file.
        int pageCount = 0;
        int pagesThatArePictures = 0;

        // True when there are words on a page that nothing here could read.
        // What to offer the user about it is a decision for the screen, which
        // is the only layer that knows whether an AI brain is connected.
        bool holdsPagesNothingCouldRead() const { return pagesThatArePictures > 0; }
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
