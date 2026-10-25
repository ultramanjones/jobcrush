#include "FormattedDocumentWriter.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

#include "ZipArchiveWriter.h"

namespace {

// Two units appear in the Word XML below:
//   half-points  font size. 22 half-points is 11pt.
//   twips        lengths and spacing. A twip is 1/20 of a point, so 1440
//                twips is one inch. Page size, margins, indents and the space
//                around paragraphs are all in twips.

// The font size of body text, bullets and checkboxes: 11pt.
constexpr int bodyTextHalfPoints = 22;

// XML rejects five characters. Without this, a cover letter containing "R&D"
// produces a .docx that Word refuses to open.
//
// "&" is replaced first. Every other replacement adds a new "&", and those
// must not be escaped a second time.
QString escapedForXml(const QString &text)
{
    QString escapedText = text;
    escapedText.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    escapedText.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    escapedText.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    escapedText.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    escapedText.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
    return escapedText;
}

// Half-points. That is the unit WordprocessingML uses for font size.
int halfPointsForHeadingLevel(int headingLevel)
{
    switch (headingLevel) {
    case 1:  return 32; // 16pt
    case 2:  return 26; // 13pt
    case 3:  return 24; // 12pt
    default: return bodyTextHalfPoints; // 11pt
    }
}

// Writes one Word "run" (<w:r>) for each piece of text. A run is a stretch
// of text with one set of formatting. Inside each run:
//   <w:rPr>   the run's formatting: <w:b/> bold, <w:i/> italic, and the size
//             twice. <w:sz> sizes most text. <w:szCs> sizes scripts such as
//             Arabic and Hebrew, which Word sizes separately.
//   <w:t>     the text itself. xml:space="preserve" keeps leading and
//             trailing spaces. Without it, Word drops them and words run
//             together where two runs meet.
// forceBold makes every run bold. Headings use it.
QString wordRunsFor(const QList<MarkdownDocumentReader::TextRun> &runs,
                    bool forceBold,
                    int halfPointSize)
{
    QString runsXml;
    for (const MarkdownDocumentReader::TextRun &run : runs) {
        if (run.text.isEmpty()) {
            continue;
        }
        runsXml += QStringLiteral("<w:r><w:rPr>");
        if (run.isBold || forceBold) {
            runsXml += QStringLiteral("<w:b/>");
        }
        if (run.isItalic) {
            runsXml += QStringLiteral("<w:i/>");
        }
        runsXml += QStringLiteral("<w:sz w:val=\"%1\"/><w:szCs w:val=\"%1\"/>")
                       .arg(halfPointSize);
        runsXml += QStringLiteral("</w:rPr><w:t xml:space=\"preserve\">%1</w:t></w:r>")
                       .arg(escapedForXml(run.text));
    }
    return runsXml;
}

} // namespace

bool FormattedDocumentWriter::writeWordDocument(const QString &markdownText,
                                        const QString &filePath,
                                        QString &reasonText) const
{
    const QList<MarkdownDocumentReader::Block> blocks = markdownReader.blocksIn(markdownText);

    // Each block becomes one Word paragraph (<w:p>). <w:pPr> holds the
    // paragraph's layout: <w:spacing> the space before and after it, in
    // twips, and <w:ind> its indent.
    QString bodyXml;
    for (const MarkdownDocumentReader::Block &block : blocks) {
        switch (block.blockKind) {
        case MarkdownDocumentReader::BlockKind::Heading:
            // 12pt above, 6pt below. <w:keepNext/> keeps the heading on the
            // same page as the paragraph after it.
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:spacing w:before=\"240\" w:after=\"120\"/>"
                           "<w:keepNext/></w:pPr>%1</w:p>")
                           .arg(wordRunsFor(block.runs, true,
                                            halfPointsForHeadingLevel(block.headingLevel)));
            break;

        case MarkdownDocumentReader::BlockKind::Bullet:
            // A hanging indent and a bullet character instead of a Word
            // numbering definition. Word renders it the same, and the file
            // stays simple enough to read with an unzipper. The paragraph is
            // indented a quarter inch (360 twips). The first line starts
            // 180 twips further left, so the bullet hangs out in front of the
            // text.
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:ind w:left=\"360\" w:hanging=\"180\"/>"
                           "<w:spacing w:after=\"60\"/></w:pPr>"
                           "<w:r><w:rPr><w:sz w:val=\"22\"/></w:rPr>"
                           "<w:t xml:space=\"preserve\">•  </w:t></w:r>%1</w:p>")
                           .arg(wordRunsFor(block.runs, false, bodyTextHalfPoints));
            break;

        case MarkdownDocumentReader::BlockKind::CheckboxUnticked:
        case MarkdownDocumentReader::BlockKind::CheckboxTicked:
            // Laid out like a bullet, with a ticked or empty box in place of
            // the bullet character.
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:ind w:left=\"360\" w:hanging=\"180\"/>"
                           "<w:spacing w:after=\"60\"/></w:pPr>"
                           "<w:r><w:rPr><w:sz w:val=\"22\"/></w:rPr>"
                           "<w:t xml:space=\"preserve\">%1  </w:t></w:r>%2</w:p>")
                           .arg(block.blockKind
                                    == MarkdownDocumentReader::BlockKind::CheckboxTicked
                                ? QStringLiteral("☑")
                                : QStringLiteral("☐"),
                                wordRunsFor(block.runs, false, bodyTextHalfPoints));
            break;

        case MarkdownDocumentReader::BlockKind::Rule:
            // The resume starts on a new page instead of running on under
            // the signature.
            bodyXml += QStringLiteral(
                "<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>");
            break;

        case MarkdownDocumentReader::BlockKind::Paragraph:
            // 8pt after. w:line="276" with lineRule "auto" is in 240ths of a
            // line, so 276 is 1.15 line spacing, Word's own default.
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:spacing w:after=\"160\" w:line=\"276\" "
                           "w:lineRule=\"auto\"/></w:pPr>%1</w:p>")
                           .arg(wordRunsFor(block.runs, false, bodyTextHalfPoints));
            break;
        }
    }

    // A .docx needs at least these three files in a ZIP:
    //   word/document.xml    the text, built above
    //   [Content_Types].xml  says what kind of data each file holds
    //   _rels/.rels          says which file is the main document
    //
    // <w:sectPr> sets the page: US Letter (12240 x 15840 twips, 8.5 x 11
    // inches) with one-inch margins (1440 twips) on every side.
    const QString documentXml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>%1"
        "<w:sectPr><w:pgSz w:w=\"12240\" w:h=\"15840\"/>"
        "<w:pgMar w:top=\"1440\" w:right=\"1440\" w:bottom=\"1440\" w:left=\"1440\"/>"
        "</w:sectPr></w:body></w:document>")
        .arg(bodyXml);

    const QString contentTypesXml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd."
        "openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "</Types>");

    const QString packageRelationshipsXml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/"
        "2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>");

    ZipArchiveWriter archiveWriter;
    archiveWriter.addStoredFile(QStringLiteral("[Content_Types].xml"), contentTypesXml.toUtf8());
    archiveWriter.addStoredFile(QStringLiteral("_rels/.rels"), packageRelationshipsXml.toUtf8());
    archiveWriter.addStoredFile(QStringLiteral("word/document.xml"), documentXml.toUtf8());

    QFile wordFile(filePath);
    if (!wordFile.open(QIODevice::WriteOnly)) {
        reasonText = QStringLiteral("Job Crush couldn't write %1 — %2")
                         .arg(QFileInfo(filePath).fileName(), wordFile.errorString());
        qWarning().noquote() << QStringLiteral(
            "FormattedDocumentWriter::writeWordDocument: could not open \"%1\" — %2")
                .arg(filePath, wordFile.errorString());
        return false;
    }
    const QByteArray archiveBytes = archiveWriter.archiveBytes();
    if (wordFile.write(archiveBytes) != archiveBytes.size()) {
        qWarning().noquote() << QStringLiteral(
            "FormattedDocumentWriter::writeWordDocument: could not write all of \"%1\" — %2")
                .arg(filePath, wordFile.errorString());
    }
    wordFile.close();
    return true;
}

bool FormattedDocumentWriter::writePortableDocument(const QString &markdownText,
                                            const QString &filePath,
                                            QString &reasonText) const
{
    // Qt reads markdown and lays it out, including page breaks. No reason to
    // draw the PDF by hand. The page is US Letter with 20 mm margins.
    QTextDocument renderedDocument;
    renderedDocument.setMarkdown(markdownText, QTextDocument::MarkdownDialectCommonMark);

    QPdfWriter pdfWriter(filePath);
    pdfWriter.setPageSize(QPageSize(QPageSize::Letter));
    pdfWriter.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    pdfWriter.setTitle(QFileInfo(filePath).completeBaseName());
    pdfWriter.setCreator(QStringLiteral("Job Crush"));

    renderedDocument.setPageSize(QSizeF(pdfWriter.width(), pdfWriter.height()));
    renderedDocument.print(&pdfWriter);

    // QTextDocument::print reports no errors. The only check is whether the
    // file is there afterwards.
    if (!QFileInfo::exists(filePath)) {
        reasonText = QStringLiteral("Job Crush couldn't write %1.")
                         .arg(QFileInfo(filePath).fileName());
        qWarning().noquote() << QStringLiteral(
            "FormattedDocumentWriter::writePortableDocument: \"%1\" is not there after"
            " printing — the PDF writer could not create it").arg(filePath);
        return false;
    }
    return true;
}
