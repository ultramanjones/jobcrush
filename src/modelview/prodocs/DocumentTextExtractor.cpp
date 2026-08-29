#include "DocumentTextExtractor.h"

#include <QFile>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QTextStream>

#include "DroppedFileTypes.h"
#include "PlainTextNormalizer.h"
#include "WordDocumentReader.h"

namespace {

// Below this, a PDF is a picture of a page rather than a page of words.
// Chosen low on purpose: a genuinely short resume should never be accused of
// being a scan, but a scanned one yields almost nothing and must be caught.
constexpr int fewestCharactersThatCountAsRealText = 40;

// The same question asked of ONE page. A scanned page usually gives back
// nothing at all, sometimes a stray character the renderer found in a margin,
// so the bar is low but not zero.
constexpr int fewestCharactersThatCountAsAPageOfWords = 25;

} // namespace

DocumentTextExtractor::ExtractionResult
DocumentTextExtractor::extractFromPlainTextFile(const QString &filePath) const
{
    QFile textFile(filePath);
    if (!textFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return { QString(), QStringLiteral("Job Crush couldn't open that file to read it.") };
    }
    QTextStream textStream(&textFile);
    return { textStream.readAll(), QString() };
}

DocumentTextExtractor::ExtractionResult
DocumentTextExtractor::extractFromPortableDocument(const QString &filePath) const
{
    QPdfDocument portableDocument;
    if (portableDocument.load(filePath) != QPdfDocument::Error::None) {
        return { QString(),
                 QStringLiteral("That PDF wouldn't open — it may be password "
                                "protected or damaged.") };
    }

    // Counted page by page, not added up and judged at the end.
    //
    // A five-page PDF where page one is a real text cover sheet and pages two
    // to five are scans clears any whole-document threshold easily, and would
    // be reported as read with four fifths of it silently missing. That is
    // worse than a document that fails outright, because a full scan at least
    // tells the user something is wrong.
    QString allPagesText;
    int pagesWithWords = 0;
    int pagesThatArePictures = 0;

    for (int pageNumber = 0; pageNumber < portableDocument.pageCount(); ++pageNumber) {
        const QPdfSelection wholePage = portableDocument.getAllText(pageNumber);
        const QString pageText = wholePage.text();

        if (pageText.trimmed().length() >= fewestCharactersThatCountAsAPageOfWords) {
            ++pagesWithWords;
        } else {
            ++pagesThatArePictures;
        }

        if (!pageText.isEmpty()) {
            allPagesText += pageText;
            allPagesText += QStringLiteral("\n");
        }
    }

    ExtractionResult result;
    result.extractedText = allPagesText;
    result.pageCount = portableDocument.pageCount();
    result.pagesThatArePictures = pagesThatArePictures;

    // Nothing readable anywhere. The whole file is a photograph of paper.
    //
    // Wording matters here. An earlier version said "a version exported from
    // Word or Google Docs will come in properly", which reads as "put THIS
    // file through Google Docs and re-download it" — a round trip that adds no
    // text and wastes the user's time. The fix is to find the ORIGINAL digital
    // document, and the copy has to say that without leaving room for the
    // other reading.
    if (pagesWithWords == 0
        || allPagesText.trimmed().length() < fewestCharactersThatCountAsRealText) {
        result.pagesThatArePictures = result.pageCount;
        result.note =
            QStringLiteral("There's no text inside this PDF — it's a picture of the "
                           "page, not words, so there's nothing for Job Crush to read "
                           "on its own. It's saved either way.  If you have the "
                           "original digital copy — the file it was written in, or one "
                           "you downloaded from a website or an email — drop that in "
                           "too and it'll come straight through. Putting this scan "
                           "through Word or Google Docs won't add text to it.");
        return result;
    }

    // Some of it read and some of it did not. Say which, and say how much:
    // "some pages" leaves the user guessing whether the missing part was the
    // job they most want counted.
    if (pagesThatArePictures > 0) {
        result.note =
            QStringLiteral("%1 of the %2 pages in this PDF are pictures rather than "
                           "words, so Job Crush read the rest and could not read those. "
                           "Anything on them is missing from your experience and "
                           "schooling.")
                .arg(pagesThatArePictures)
                .arg(result.pageCount);
        return result;
    }

    return result;
}

DocumentTextExtractor::ExtractionResult
DocumentTextExtractor::extractFromWordDocument(const QString &filePath) const
{
    const WordDocumentReader wordDocumentReader;
    QString reasonText;
    const QString documentText = wordDocumentReader.readTextFrom(filePath, reasonText);
    return { documentText, reasonText };
}

DocumentTextExtractor::ExtractionResult
DocumentTextExtractor::extractTextFrom(const QString &filePath) const
{
    // One door in, one alphabet out. Every reader below hands its result
    // through the normalizer, so nothing downstream ever meets a non-breaking
    // space or somebody's favourite bullet glyph.
    ExtractionResult normalizedResult = extractTextFromFileByKind(filePath);
    normalizedResult.extractedText =
        withEveryLookalikeCharacterNormalized(normalizedResult.extractedText);
    return normalizedResult;
}

DocumentTextExtractor::ExtractionResult
DocumentTextExtractor::extractTextFromFileByKind(const QString &filePath) const
{
    const QString extension = DroppedFileTypes::loweredExtensionOf(filePath);

    if (extension == QStringLiteral("txt")
            || extension == QStringLiteral("md")
            || extension == QStringLiteral("markdown")) {
        return extractFromPlainTextFile(filePath);
    }

    if (extension == QStringLiteral("pdf")) {
        return extractFromPortableDocument(filePath);
    }

    if (extension == QStringLiteral("docx")) {
        return extractFromWordDocument(filePath);
    }

    if (DroppedFileTypes::isImage(filePath)) {
        // Not a failure. An image was never going to have words in it, and
        // Job Crush keeps it for a profile picture or a letterhead.
        return { QString(), QStringLiteral("Kept as a picture — there's no text in an "
                                           "image to read.") };
    }

    // .docx, .rtf and .odt are stored safely and read once their formats land.
    // Never a bare "can't" — the user gets the one-step way to be read today.
    return { QString(),
             QStringLiteral("Saved, but Job Crush can't read .%1 files yet, so nothing "
                            "has been pulled out of this one. To have it read now: open "
                            "it, choose Save As or Export, and pick PDF — then drop that "
                            "in. It takes about ten seconds.").arg(extension) };
}
