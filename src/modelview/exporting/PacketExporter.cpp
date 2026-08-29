#include "PacketExporter.h"

#include <QDir>
#include <QFileInfo>


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
        ? documentWriter.writePortableDocument(markdownText, filePath, reasonText)
        : documentWriter.writeWordDocument(markdownText, filePath, reasonText);

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
