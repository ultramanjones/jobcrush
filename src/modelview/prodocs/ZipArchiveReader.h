#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// ZipArchiveReader
//
// Pulls one named file out of a ZIP archive.
//
// It exists because a .docx IS a ZIP archive full of XML, and Qt ships no
// public ZIP reader. The same reader is what the Phase 5 packet export will
// need to WRITE, so the format has to be understood here either way.
//
// Deliberately minimal: it reads the central directory, finds an entry by
// name, and decompresses it. No writing, no encryption, no spanned archives,
// no ZIP64. Those are real parts of the format and none of them appear in a
// Word document.
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
