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

    QString allPagesText;
    for (int pageNumber = 0; pageNumber < portableDocument.pageCount(); ++pageNumber) {
        const QPdfSelection wholePage = portableDocument.getAllText(pageNumber);
        if (!wholePage.text().isEmpty()) {
            allPagesText += wholePage.text();
            allPagesText += QStringLiteral("\n");
        }
    }

    if (allPagesText.trimmed().length() < fewestCharactersThatCountAsRealText) {
        // A scan is a photograph of words, and there is nothing in the file to
        // read. Say so — otherwise the user believes their experience loaded,
        // wonders later why nothing matches, and blames the wrong thing.
        // Wording matters here. An earlier version said "a version exported
        // from Word or Google Docs will come in properly", which reads as
        // "put THIS file through Google Docs and re-download it" — a round
        // trip that adds no text and wastes the user's time. The fix is to
        // find the ORIGINAL digital document, and the copy has to say that
        // without leaving room for the other reading.
        return { allPagesText,
                 QStringLiteral("There's no text inside this PDF — it's a picture of "
                                "the page, not words, so there's nothing for Job Crush "
                                "to read. It's saved either way.  If you have the "
                                "original digital copy — the file it was written in, or "
                                "one you downloaded from a website or an email — drop "
                                "that in too and it'll come straight through. Putting "
                                "this scan through Word or Google Docs won't add text "
                                "to it.  And if a scan is all you have, type the "
                                "important parts into Experience & Education yourself — "
                                "Job Crush will use those just the same.") };
    }
    return { allPagesText, QString() };
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
    const ExtractionResult rawResult = extractTextFromFileByKind(filePath);
    return { withEveryLookalikeCharacterNormalized(rawResult.extractedText),
             rawResult.note };
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
