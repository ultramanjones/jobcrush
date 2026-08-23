#include "WordDocumentReader.h"

#include <QRegularExpression>

#include "ZipArchiveReader.h"

namespace {

// Where Word keeps the body text inside the archive. Always this path — it is
// fixed by the Office Open XML standard, not chosen by whoever saved the file.
const QString wordBodyEntryName = QStringLiteral("word/document.xml");

// Turns Word's XML into plain readable text.
//
// The rules that matter:
//   <w:t>…</w:t>   holds the actual characters
//   <w:p>          ends a paragraph — becomes a line break
//   <w:tab/>       becomes a tab, which keeps table-ish layouts readable
//   <w:br/>        a line break inside a paragraph
// Everything else is styling and is discarded.
QString plainTextFromWordXml(const QString &documentXml)
{
    QString workingText = documentXml;

    // Structure first, while the tags are still there to recognise.
    static const QRegularExpression paragraphEndPattern(
        QStringLiteral("</w:p>"), QRegularExpression::CaseInsensitiveOption);
    workingText.replace(paragraphEndPattern, QStringLiteral("\n"));

    static const QRegularExpression lineBreakPattern(
        QStringLiteral("<w:br\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    workingText.replace(lineBreakPattern, QStringLiteral("\n"));

    static const QRegularExpression tabPattern(
        QStringLiteral("<w:tab\\s*/?>"), QRegularExpression::CaseInsensitiveOption);
    workingText.replace(tabPattern, QStringLiteral("\t"));

    // A table cell ending is a column boundary — worth a tab so a transcript
    // laid out as a table doesn't come out as one long run of words.
    static const QRegularExpression tableCellEndPattern(
        QStringLiteral("</w:tc>"), QRegularExpression::CaseInsensitiveOption);
    workingText.replace(tableCellEndPattern, QStringLiteral("\t"));

    static const QRegularExpression tableRowEndPattern(
        QStringLiteral("</w:tr>"), QRegularExpression::CaseInsensitiveOption);
    workingText.replace(tableRowEndPattern, QStringLiteral("\n"));

    // Then every remaining tag goes.
    static const QRegularExpression anyTagPattern(QStringLiteral("<[^>]*>"));
    workingText.remove(anyTagPattern);

    // XML's five escapes, plus the non-breaking space Word is fond of.
    workingText.replace(QStringLiteral("&amp;"),  QStringLiteral("&"));
    workingText.replace(QStringLiteral("&lt;"),   QStringLiteral("<"));
    workingText.replace(QStringLiteral("&gt;"),   QStringLiteral(">"));
    workingText.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    workingText.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    workingText.replace(QChar(0x00A0), QLatin1Char(' '));

    // Tidy the blank space that stripping always leaves behind.
    static const QRegularExpression trailingSpacePattern(QStringLiteral("[ \\t]+\\n"));
    workingText.replace(trailingSpacePattern, QStringLiteral("\n"));
    static const QRegularExpression runOfBlankLinesPattern(QStringLiteral("\\n{3,}"));
    workingText.replace(runOfBlankLinesPattern, QStringLiteral("\n\n"));

    return workingText.trimmed();
}

} // namespace

QString WordDocumentReader::readTextFrom(const QString &wordDocumentFilePath,
                                         QString &reasonText) const
{
    reasonText.clear();

    const ZipArchiveReader archiveReader(wordDocumentFilePath);
    if (!archiveReader.isOpen()) {
        reasonText = QStringLiteral("Job Crush couldn't open that Word file — it may be "
                                    "damaged, or still downloading. Try opening it in "
                                    "Word, saving it again, and dropping the new copy in.");
        return QString();
    }

    if (!archiveReader.containsEntry(wordBodyEntryName)) {
        // A .docx that has no word/document.xml is almost always something
        // else wearing the extension — most often an old .doc renamed.
        reasonText = QStringLiteral("That file is named .docx but isn't shaped like a "
                                    "Word document inside. If it started life as an "
                                    "older .doc, open it in Word and use Save As to make "
                                    "a real .docx or a PDF, then drop that in.");
        return QString();
    }

    bool found = false;
    const QByteArray documentXmlBytes = archiveReader.readEntry(wordBodyEntryName, found);
    if (!found || documentXmlBytes.isEmpty()) {
        reasonText = QStringLiteral("Job Crush found the text inside that Word file but "
                                    "couldn't unpack it. Opening it in Word and saving a "
                                    "fresh copy usually sorts it out — or export it as a "
                                    "PDF and drop that in instead.");
        return QString();
    }

    const QString documentText = plainTextFromWordXml(QString::fromUtf8(documentXmlBytes));
    if (documentText.isEmpty()) {
        reasonText = QStringLiteral("That Word document opened, but there are no words "
                                    "in it — it may be empty, or everything in it may be "
                                    "a picture.");
    }
    return documentText;
}
