#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// ZipArchiveWriter
//
// Writes a ZIP file, one entry at a time. A .docx file is a ZIP file, so this
// is what writes .docx. About a hundred lines, which is cheaper than adding a
// library.
//
// Every entry is stored uncompressed. That is a normal ZIP and every unzipper
// and every version of Word opens it. The only cost is file size, and a packet
// is a few kilobytes. Writing DEFLATE compression would be real work for no
// benefit here.
//
// Job Crush does implement DEFLATE for reading, in DeflateDecompressor, since
// the .docx files users drop on ProDocs are compressed.
//
// Small amount of state, so it stays header-only.
class ZipArchiveWriter {
public:
    void addStoredFile(const QString &pathInsideArchive, const QByteArray &fileContents)
    {
        Entry entry;
        entry.pathInsideArchive = pathInsideArchive.toUtf8();
        entry.fileContents = fileContents;
        entry.checksum = crc32Of(fileContents);
        entry.offsetOfLocalHeader = assembledBytes.size();

        assembledBytes.append(localFileHeaderFor(entry));
        assembledBytes.append(fileContents);

        entries.append(entry);
    }

    // The finished archive. Call after all entries have been added.
    QByteArray archiveBytes() const
    {
        QByteArray finishedArchive = assembledBytes;

        const int startOfCentralDirectory = finishedArchive.size();
        for (const Entry &entry : entries) {
            finishedArchive.append(centralDirectoryHeaderFor(entry));
        }
        const int centralDirectorySize = finishedArchive.size() - startOfCentralDirectory;

        QByteArray endRecord;
        appendLittleEndian32(endRecord, 0x06054b50);        // end of central directory
        appendLittleEndian16(endRecord, 0);                 // this disk number
        appendLittleEndian16(endRecord, 0);                 // disk with central directory
        appendLittleEndian16(endRecord, entries.count());   // entries on this disk
        appendLittleEndian16(endRecord, entries.count());   // entries total
        appendLittleEndian32(endRecord, centralDirectorySize);
        appendLittleEndian32(endRecord, startOfCentralDirectory);
        appendLittleEndian16(endRecord, 0);                 // no archive comment
        finishedArchive.append(endRecord);

        return finishedArchive;
    }

private:
    struct Entry {
        QByteArray pathInsideArchive;
        QByteArray fileContents;
        quint32 checksum = 0;
        int offsetOfLocalHeader = 0;
    };

    static void appendLittleEndian16(QByteArray &target, quint16 value)
    {
        target.append(static_cast<char>(value & 0xFF));
        target.append(static_cast<char>((value >> 8) & 0xFF));
    }

    static void appendLittleEndian32(QByteArray &target, quint32 value)
    {
        target.append(static_cast<char>(value & 0xFF));
        target.append(static_cast<char>((value >> 8) & 0xFF));
        target.append(static_cast<char>((value >> 16) & 0xFF));
        target.append(static_cast<char>((value >> 24) & 0xFF));
    }

    static QByteArray localFileHeaderFor(const Entry &entry)
    {
        QByteArray header;
        appendLittleEndian32(header, 0x04034b50);   // local file header
        appendLittleEndian16(header, 20);           // version needed to extract
        appendLittleEndian16(header, 0);            // flags
        appendLittleEndian16(header, 0);            // method 0 = stored
        appendLittleEndian16(header, 0);            // modification time
        appendLittleEndian16(header, 0x21);         // modification date (1st Jan 1980)
        appendLittleEndian32(header, entry.checksum);
        appendLittleEndian32(header, entry.fileContents.size());  // compressed size
        appendLittleEndian32(header, entry.fileContents.size());  // uncompressed size
        appendLittleEndian16(header, entry.pathInsideArchive.size());
        appendLittleEndian16(header, 0);            // no extra field
        header.append(entry.pathInsideArchive);
        return header;
    }

    static QByteArray centralDirectoryHeaderFor(const Entry &entry)
    {
        QByteArray header;
        appendLittleEndian32(header, 0x02014b50);   // central directory header
        appendLittleEndian16(header, 20);           // version made by
        appendLittleEndian16(header, 20);           // version needed
        appendLittleEndian16(header, 0);            // flags
        appendLittleEndian16(header, 0);            // method 0 = stored
        appendLittleEndian16(header, 0);            // modification time
        appendLittleEndian16(header, 0x21);         // modification date
        appendLittleEndian32(header, entry.checksum);
        appendLittleEndian32(header, entry.fileContents.size());
        appendLittleEndian32(header, entry.fileContents.size());
        appendLittleEndian16(header, entry.pathInsideArchive.size());
        appendLittleEndian16(header, 0);            // extra field length
        appendLittleEndian16(header, 0);            // comment length
        appendLittleEndian16(header, 0);            // disk number
        appendLittleEndian16(header, 0);            // internal attributes
        appendLittleEndian32(header, 0);            // external attributes
        appendLittleEndian32(header, entry.offsetOfLocalHeader);
        header.append(entry.pathInsideArchive);
        return header;
    }

    // The standard CRC-32 every ZIP uses. The table is built once.
    static quint32 crc32Of(const QByteArray &data)
    {
        static quint32 checksumTable[256];
        static bool tableIsBuilt = false;
        if (!tableIsBuilt) {
            for (quint32 tableIndex = 0; tableIndex < 256; ++tableIndex) {
                quint32 value = tableIndex;
                for (int bitNumber = 0; bitNumber < 8; ++bitNumber) {
                    value = (value & 1u) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
                }
                checksumTable[tableIndex] = value;
            }
            tableIsBuilt = true;
        }

        quint32 checksum = 0xFFFFFFFFu;
        for (const char byteValue : data) {
            checksum = checksumTable[(checksum ^ static_cast<quint8>(byteValue)) & 0xFFu]
                     ^ (checksum >> 8);
        }
        return checksum ^ 0xFFFFFFFFu;
    }

    QByteArray assembledBytes;
    QList<Entry> entries;
};
