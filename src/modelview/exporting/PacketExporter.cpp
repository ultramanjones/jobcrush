#include "PacketExporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPageSize>
#include <QPdfWriter>
#include <QTextDocument>

#include "ZipArchiveWriter.h"

namespace {

// XML rejects five characters. Without this, a cover letter containing "R&D"
// produces a .docx that Word refuses to open.
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
    default: return 22; // 11pt
    }
}

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

QString PacketExporter::assembledMarkdownFor(const QList<StagedDocument> &packet) const
{
    QString assembledText;

    // The repository already returned the list in packet order, which is
    // defined in StagedDocument.h. Do not sort again here.
    for (const StagedDocument &piece : packet) {
        if (!belongsInTheSentPacket(piece.documentKind)) {
            continue;
        }
        if (piece.markdownText.trimmed().isEmpty()) {
            continue;
        }
        if (!assembledText.isEmpty()) {
            // A page break between the letter and the resume. Written as a
            // markdown horizontal rule, which both writers below understand.
            assembledText += QStringLiteral("\n\n---\n\n");
        }
        assembledText += piece.markdownText.trimmed();
    }
    return assembledText;
}

PacketExporter::ExportOutcome PacketExporter::exportPacket(
    const QList<StagedDocument> &packet,
    const QString &format,
    const QString &destinationFolderPath,
    const QString &suggestedFileName) const
{
    ExportOutcome outcome;

    const QString markdownText = assembledMarkdownFor(packet);
    if (markdownText.trimmed().isEmpty()) {
        outcome.reasonText =
            QStringLiteral("There's nothing in this packet to send yet.");
        outcome.whatToDoNextText =
            QStringLiteral("A packet goes out as a cover letter and a resume. Draft "
                           "one of those first and this button will have something "
                           "to write.");
        return outcome;
    }

    QDir destinationFolder(destinationFolderPath);
    if (!destinationFolder.exists() && !destinationFolder.mkpath(QStringLiteral("."))) {
        outcome.reasonText = QStringLiteral("Job Crush couldn't create the folder %1.")
                                 .arg(destinationFolderPath);
        outcome.whatToDoNextText =
            QStringLiteral("Pick a different folder, or check whether that one is "
                           "somewhere you're allowed to write.");
        return outcome;
    }

    const QString chosenFormat =
        ExportFormat::isKnownFormat(format) ? format : ExportFormat::WordDocument;
    const QString fileName = QStringLiteral("%1.%2")
                                 .arg(suggestedFileName,
                                      ExportFormat::fileExtensionFor(chosenFormat));
    const QString filePath = destinationFolder.filePath(fileName);

    QString reasonText;
    const bool wasWritten = (chosenFormat == ExportFormat::PortableDocument)
        ? writePortableDocument(markdownText, filePath, reasonText)
        : writeWordDocument(markdownText, filePath, reasonText);

    if (!wasWritten) {
        outcome.reasonText = reasonText;
        outcome.whatToDoNextText =
            QStringLiteral("If the file is open in another program, close it and try "
                           "again — Windows won't let anything overwrite a file that "
                           "is already open.");
        return outcome;
    }

    outcome.succeeded = true;
    outcome.writtenFilePath = filePath;
    return outcome;
}

bool PacketExporter::writeWordDocument(const QString &markdownText,
                                        const QString &filePath,
                                        QString &reasonText) const
{
    const QList<MarkdownDocumentReader::Block> blocks = markdownReader.blocksIn(markdownText);

    QString bodyXml;
    for (const MarkdownDocumentReader::Block &block : blocks) {
        switch (block.blockKind) {
        case MarkdownDocumentReader::BlockKind::Heading:
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:spacing w:before=\"240\" w:after=\"120\"/>"
                           "<w:keepNext/></w:pPr>%1</w:p>")
                           .arg(wordRunsFor(block.runs, true,
                                            halfPointsForHeadingLevel(block.headingLevel)));
            break;

        case MarkdownDocumentReader::BlockKind::Bullet:
            // A hanging indent and a bullet character instead of a Word
            // numbering definition. Word renders it the same, and the file
            // stays simple enough to read with an unzipper.
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:ind w:left=\"360\" w:hanging=\"180\"/>"
                           "<w:spacing w:after=\"60\"/></w:pPr>"
                           "<w:r><w:rPr><w:sz w:val=\"22\"/></w:rPr>"
                           "<w:t xml:space=\"preserve\">•  </w:t></w:r>%1</w:p>")
                           .arg(wordRunsFor(block.runs, false, 22));
            break;

        case MarkdownDocumentReader::BlockKind::CheckboxUnticked:
        case MarkdownDocumentReader::BlockKind::CheckboxTicked:
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:ind w:left=\"360\" w:hanging=\"180\"/>"
                           "<w:spacing w:after=\"60\"/></w:pPr>"
                           "<w:r><w:rPr><w:sz w:val=\"22\"/></w:rPr>"
                           "<w:t xml:space=\"preserve\">%1  </w:t></w:r>%2</w:p>")
                           .arg(block.blockKind
                                    == MarkdownDocumentReader::BlockKind::CheckboxTicked
                                ? QStringLiteral("☑")
                                : QStringLiteral("☐"),
                                wordRunsFor(block.runs, false, 22));
            break;

        case MarkdownDocumentReader::BlockKind::Rule:
            // The resume starts on a new page instead of running on under
            // the signature.
            bodyXml += QStringLiteral(
                "<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>");
            break;

        case MarkdownDocumentReader::BlockKind::Paragraph:
            bodyXml += QStringLiteral(
                           "<w:p><w:pPr><w:spacing w:after=\"160\" w:line=\"276\" "
                           "w:lineRule=\"auto\"/></w:pPr>%1</w:p>")
                           .arg(wordRunsFor(block.runs, false, 22));
            break;
        }
    }

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
        return false;
    }
    wordFile.write(archiveWriter.archiveBytes());
    wordFile.close();
    return true;
}

bool PacketExporter::writePortableDocument(const QString &markdownText,
                                            const QString &filePath,
                                            QString &reasonText) const
{
    // Qt reads markdown and lays it out, including page breaks. No reason to
    // draw the PDF by hand.
    QTextDocument renderedDocument;
    renderedDocument.setMarkdown(markdownText, QTextDocument::MarkdownDialectCommonMark);

    QPdfWriter pdfWriter(filePath);
    pdfWriter.setPageSize(QPageSize(QPageSize::Letter));
    pdfWriter.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    pdfWriter.setTitle(QFileInfo(filePath).completeBaseName());
    pdfWriter.setCreator(QStringLiteral("Job Crush"));

    renderedDocument.setPageSize(QSizeF(pdfWriter.width(), pdfWriter.height()));
    renderedDocument.print(&pdfWriter);

    if (!QFileInfo::exists(filePath)) {
        reasonText = QStringLiteral("Job Crush couldn't write %1.")
                         .arg(QFileInfo(filePath).fileName());
        return false;
    }
    return true;
}
