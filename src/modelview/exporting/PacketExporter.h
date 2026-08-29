#pragma once

#include <QList>
#include <QString>

#include "../../model/StagedDocument.h"
#include "ExportFormat.h"
#include "FormattedDocumentWriter.h"

// PacketExporter
//
// Turns a packet into one file the user can send.
//
// One file, not a folder and not a zip. The cover letter is first, then the
// tailored resume.
//
// Only pieces marked as going to the employer are included. See
// belongsInTheSentPacket. The checklist and the fit score stay out. The
// employer should not receive the app's guess at the user's odds.
//
// A ModelView class. It knows about documents and file formats, and nothing
// about screens.
class PacketExporter {
public:
    struct ExportOutcome {
        bool succeeded = false;
        QString writtenFilePath;
        QString reasonText;        // why not, in words for a person
        QString whatToDoNextText;  // and what they can do about it
    };

    // Writes the packet to a file. suggestedFileName is used as given; the
    // caller has already removed characters the filesystem rejects. The
    // extension comes from the format.
    ExportOutcome exportPacket(const QList<StagedDocument> &packet,
                               const QString &format,
                               const QString &destinationFolderPath,
                               const QString &suggestedFileName) const;

    // The whole packet as markdown, in packet order. Public so the preview
    // can show exactly what will be written, using this same code.
    QString assembledMarkdownFor(const QList<StagedDocument> &packet) const;

private:
    FormattedDocumentWriter documentWriter;
};
