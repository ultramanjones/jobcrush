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
// The layout it writes, from the start of the file to the end:
//
//   [local file header 1][file 1 bytes]
//   [local file header 2][file 2 bytes]
//   ...
//   [central directory header 1]     the index: one header per file, each
//   [central directory header 2]     saying where that file's local header
//   ...                              starts
//   [end of central directory record]  how many files, and where the index is
//
// The files are written as they are added. The index and the end record
// can only be written once every file is in, so archiveBytes() adds them
// at the end. Field layouts are from the ZIP specification (PKWARE APPNOTE,
// section 4.3). Every number is little-endian (lowest byte first).
//
// There is no ZIP64 support, so the archive must stay under 4 GB and 65535
// entries. A packet is far below both.
//
// Small amount of state, so it stays header-only.
class ZipArchiveWriter {
public:
    // Adds one file. Its local header and its bytes go into the archive now.
    // What the index will need later (name, checksum, size, and where the
    // local header starts) is kept in entries.
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
    //
    // Copies what was assembled so far, then adds the index (one central
    // directory header per entry) and the end record. The end record needs
    // the index's size and starting offset, so those are measured as the
    // index is written.
    QByteArray archiveBytes() const
    {
        QByteArray finishedArchive = assembledBytes;

        const int startOfCentralDirectory = finishedArchive.size();
        for (const Entry &entry : entries) {
            finishedArchive.append(centralDirectoryHeaderFor(entry));
        }
        const int centralDirectorySize = finishedArchive.size() - startOfCentralDirectory;

        // End of central directory record. 22 bytes, then the comment.
        QByteArray endRecord;
        appendLittleEndian32(endRecord, endOfCentralDirectorySignature);
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

    // The signatures that mark each ZIP structure. Written little-endian,
    // each one starts with the letters "PK" in the file.
    static constexpr quint32 localFileHeaderSignature = 0x04034b50;
    static constexpr quint32 centralDirectoryHeaderSignature = 0x02014b50;
    static constexpr quint32 endOfCentralDirectorySignature = 0x06054b50;

    // Version 2.0 of the ZIP specification, written as 20. Every unzipper
    // accepts it.
    static constexpr quint16 zipSpecificationVersion = 20;

    // A ZIP date packs the year, month and day into 16 bits:
    //   bits 9-15  years since 1980
    //   bits 5-8   month, 1 to 12
    //   bits 0-4   day, 1 to 31
    // 0x21 is year 0, month 1, day 1: 1 January 1980, the earliest date the
    // format can hold. Every file gets the same fixed date, and the time is
    // 0, midnight. So the same packet always gives the same bytes.
    static constexpr quint16 earliestZipDate = 0x21;

    // Splits a number into bytes and appends them lowest byte first.
    // "& 0xFF" keeps only the lowest 8 bits after each shift.
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

    // The header in front of each file's bytes. 30 fixed bytes, then the name.
    static QByteArray localFileHeaderFor(const Entry &entry)
    {
        QByteArray header;
        appendLittleEndian32(header, localFileHeaderSignature);
        appendLittleEndian16(header, zipSpecificationVersion); // version needed to extract
        appendLittleEndian16(header, 0);            // flags
        appendLittleEndian16(header, 0);            // method 0 = stored
        appendLittleEndian16(header, 0);            // modification time
        appendLittleEndian16(header, earliestZipDate); // modification date (1 Jan 1980)
        appendLittleEndian32(header, entry.checksum);
        appendLittleEndian32(header, entry.fileContents.size());  // compressed size
        appendLittleEndian32(header, entry.fileContents.size());  // uncompressed size
        appendLittleEndian16(header, entry.pathInsideArchive.size());
        appendLittleEndian16(header, 0);            // no extra field
        header.append(entry.pathInsideArchive);
        return header;
    }

    // One index entry. 46 fixed bytes, then the name. It repeats most of the
    // local header and adds where that local header starts.
    static QByteArray centralDirectoryHeaderFor(const Entry &entry)
    {
        QByteArray header;
        appendLittleEndian32(header, centralDirectoryHeaderSignature);
        appendLittleEndian16(header, zipSpecificationVersion); // version made by
        appendLittleEndian16(header, zipSpecificationVersion); // version needed
        appendLittleEndian16(header, 0);            // flags
        appendLittleEndian16(header, 0);            // method 0 = stored
        appendLittleEndian16(header, 0);            // modification time
        appendLittleEndian16(header, earliestZipDate); // modification date
        appendLittleEndian32(header, entry.checksum);
        appendLittleEndian32(header, entry.fileContents.size()); // compressed size
        appendLittleEndian32(header, entry.fileContents.size()); // uncompressed size
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

    // The standard CRC-32 every ZIP uses (the same one as Ethernet and PNG).
    // An unzipper works out the checksum again and refuses the file if the
    // two differ.
    //
    // How it works:
    //
    // CRC-32 treats the data as one long string of bits and divides it by a
    // fixed 33-bit number, the "polynomial", using XOR in place of
    // subtraction. The remainder is the checksum. ZIP uses the bit-reversed
    // form of the polynomial, 0xEDB88320, and feeds each byte in lowest bit
    // first.
    //
    // Building the table (done once):
    //   For each possible byte value 0-255, run the division over its 8 bits.
    //   For each bit: if the lowest bit of value is 1, shift right one and
    //   XOR in the polynomial. If it is 0, only shift right one. After 8 bits,
    //   value is what that byte adds to the checksum. Store it.
    //
    // Using the table, for each byte of the data:
    //   1. XOR the byte into the low 8 bits of the running checksum.
    //   2. Use those low 8 bits to pick a table entry.
    //   3. Shift the checksum right 8 bits and XOR in the table entry.
    // This does 8 bits of the division in one step instead of 8.
    //
    // The checksum starts as all ones and is flipped (XOR all ones) at the
    // end. The specification says so. It makes leading zero bytes change the
    // result.
    //
    // The table is a static built on first use. If two threads made their
    // first call at the same moment, both would build it at once. Today
    // only the main thread writes ZIP files.
    static quint32 crc32Of(const QByteArray &data)
    {
        static constexpr quint32 reversedCrc32Polynomial = 0xEDB88320u;

        static quint32 checksumTable[256];
        static bool tableIsBuilt = false;
        if (!tableIsBuilt) {
            for (quint32 tableIndex = 0; tableIndex < 256; ++tableIndex) {
                quint32 value = tableIndex;
                for (int bitNumber = 0; bitNumber < 8; ++bitNumber) {
                    value = (value & 1u) ? (reversedCrc32Polynomial ^ (value >> 1))
                                         : (value >> 1);
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
