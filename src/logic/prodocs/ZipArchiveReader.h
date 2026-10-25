#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// ZipArchiveReader
//
// Pulls one named file out of a ZIP archive.
//
// It exists because a .docx IS a ZIP archive full of XML, and Qt ships no
// public ZIP reader. Writing is done by ZipArchiveWriter, in exporting/.
//
// Kept small on purpose: it reads the central directory (the archive's
// index), finds an entry by name, and decompresses it. No writing, no
// encryption, no archives split across several files, no ZIP64. Those are
// real parts of the format and none of them appear in a Word document. The
// .cpp explains the file layout.
//
// The whole file is read into memory when the reader is made. A .docx is
// small, so that is fine.
class ZipArchiveReader {
public:
    explicit ZipArchiveReader(const QString &archiveFilePath);

    // True when the file opened and looked like a ZIP archive.
    bool isOpen() const { return archiveIsUsable; }

    // Every entry name in the archive, for finding one's way around.
    QStringList entryNames() const;

    bool containsEntry(const QString &entryName) const;

    // The decompressed contents of one entry. Empty when it isn't there or
    // couldn't be decompressed; found says which.
    QByteArray readEntry(const QString &entryName, bool &found) const;

    QString lastErrorText() const { return lastErrorDescription; }

private:
    struct ArchiveEntry {
        QString entryName;
        int compressionMethod = 0;      // 0 = stored, 8 = deflate
        qint64 compressedSize = 0;
        qint64 uncompressedSize = 0;
        qint64 localHeaderOffset = 0;
    };

    bool readCentralDirectory();

    QString archivePath;
    QByteArray archiveBytes;
    QList<ArchiveEntry> archiveEntries;
    bool archiveIsUsable = false;
    QString lastErrorDescription;
};
