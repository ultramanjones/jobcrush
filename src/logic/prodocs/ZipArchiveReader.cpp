#include "ZipArchiveReader.h"

#include <QDebug>
#include <QFile>

#include "DeflateDecompressor.h"

// How a ZIP file is laid out
//
// From the start of the file to the end:
//
//   [local file header 1][file data 1]
//   [local file header 2][file data 2]
//   ...
//   [central directory entry 1]
//   [central directory entry 2]
//   ...
//   [end of central directory record][comment, 0 to 65535 bytes]
//
// The central directory is the archive's index. It lists every entry: its
// name, its size, how it is compressed, and where its local header starts.
// The end record says how many entries the index has and where it starts.
//
// So a reader starts at the END. It finds the end record, uses it to find
// the index, and uses the index to find each file. Every number is stored
// little-endian (lowest byte first) and unsigned.
//
// The byte offsets below are from the ZIP specification (PKWARE APPNOTE,
// section 4.3).

namespace {

// ZIP stores its numbers little-endian, unsigned, at fixed offsets. These
// two readers keep that fact in one place instead of scattered arithmetic.
// Both return 0 when the bytes asked for run past the end of the data.

// Two bytes: the low byte, then the high byte shifted up 8 bits.
quint16 readUnsigned16(const QByteArray &bytes, int offset)
{
    if (offset < 0 || offset + 1 >= bytes.size()) {
        return 0;
    }
    return static_cast<quint16>(static_cast<unsigned char>(bytes.at(offset)))
         | (static_cast<quint16>(static_cast<unsigned char>(bytes.at(offset + 1))) << 8);
}

// Four bytes, lowest first. Each one is shifted up 8 more bits than the last.
quint32 readUnsigned32(const QByteArray &bytes, int offset)
{
    if (offset < 0 || offset + 3 >= bytes.size()) {
        return 0;
    }
    return static_cast<quint32>(static_cast<unsigned char>(bytes.at(offset)))
         | (static_cast<quint32>(static_cast<unsigned char>(bytes.at(offset + 1))) << 8)
         | (static_cast<quint32>(static_cast<unsigned char>(bytes.at(offset + 2))) << 16)
         | (static_cast<quint32>(static_cast<unsigned char>(bytes.at(offset + 3))) << 24);
}

// The signatures that mark each ZIP structure. Read little-endian, each one
// starts with the letters "PK" in the file.
constexpr quint32 endOfCentralDirectorySignature = 0x06054b50;
constexpr quint32 centralDirectoryEntrySignature = 0x02014b50;
constexpr quint32 localFileHeaderSignature       = 0x04034b50;

// A ZIP file is read BACKWARDS: the index lives at the end, after a comment
// of unknown length, so the only way in is to scan back for its signature.
constexpr int longestPossibleArchiveComment = 65535;

// --- End of central directory record --------------------------------------
constexpr int endRecordFixedSize = 22;               // bytes before the comment
constexpr int endRecordEntryCountOffset = 10;        // 2 bytes: entries in total
constexpr int endRecordDirectoryStartOffset = 16;    // 4 bytes: where the index starts

// --- One central directory entry ------------------------------------------
constexpr int directoryEntryFixedSize = 46;          // bytes before the name
constexpr int directoryEntryMethodOffset = 10;       // 2 bytes: compression method
constexpr int directoryEntryCompressedSizeOffset = 20;   // 4 bytes
constexpr int directoryEntryUncompressedSizeOffset = 24; // 4 bytes
constexpr int directoryEntryNameLengthOffset = 28;   // 2 bytes
constexpr int directoryEntryExtraLengthOffset = 30;  // 2 bytes
constexpr int directoryEntryCommentLengthOffset = 32; // 2 bytes
constexpr int directoryEntryLocalHeaderOffset = 42;  // 4 bytes: where the local header starts

// --- One local file header ------------------------------------------------
constexpr int localHeaderFixedSize = 30;             // bytes before the name
constexpr int localHeaderNameLengthOffset = 26;      // 2 bytes
constexpr int localHeaderExtraLengthOffset = 28;     // 2 bytes

// --- Compression methods --------------------------------------------------
constexpr int storedCompressionMethod = 0;           // the bytes as they are
constexpr int deflateCompressionMethod = 8;          // see DeflateDecompressor

} // namespace

ZipArchiveReader::ZipArchiveReader(const QString &archiveFilePath)
    : archivePath(archiveFilePath)
{
    QFile archiveFile(archiveFilePath);
    if (!archiveFile.open(QIODevice::ReadOnly)) {
        lastErrorDescription = QStringLiteral("Couldn't open the file to read it.");
        qWarning().noquote() << QStringLiteral(
            "ZipArchiveReader::ZipArchiveReader: could not open \"%1\" — %2")
                .arg(archiveFilePath, archiveFile.errorString());
        return;
    }
    archiveBytes = archiveFile.readAll();
    archiveFile.close();

    archiveIsUsable = readCentralDirectory();
}

// Fills archiveEntries from the archive's index.
//
// Step 1. Find the end record. It is the last thing in the file, except for
//         a comment of up to 65535 bytes. So start 22 bytes from the end
//         (where the record sits when there is no comment) and step back
//         one byte at a time until its signature turns up, or the longest
//         possible comment has been passed.
// Step 2. Read from the end record how many entries there are and where
//         the index starts.
// Step 3. Walk the index. Each entry is 46 fixed bytes, then the name, then
//         an "extra" field, then a comment. The three lengths are in the
//         fixed part, so the next entry starts at
//         46 + name length + extra length + comment length.
bool ZipArchiveReader::readCentralDirectory()
{
    // Step 1. Scan backwards for the end-of-central-directory record.
    const int searchFloor =
        qMax(0, archiveBytes.size() - longestPossibleArchiveComment - endRecordFixedSize);
    int endRecordOffset = -1;
    for (int offset = archiveBytes.size() - endRecordFixedSize; offset >= searchFloor; --offset) {
        if (readUnsigned32(archiveBytes, offset) == endOfCentralDirectorySignature) {
            endRecordOffset = offset;
            break;
        }
    }
    if (endRecordOffset < 0) {
        lastErrorDescription = QStringLiteral("That file isn't a ZIP archive.");
        qWarning().noquote() << QStringLiteral(
            "ZipArchiveReader::readCentralDirectory: no end record in \"%1\""
            " — the file is not a ZIP archive").arg(archivePath);
        return false;
    }

    // Step 2. How many entries, and where the index starts.
    const quint16 entryCount =
        readUnsigned16(archiveBytes, endRecordOffset + endRecordEntryCountOffset);
    const quint32 centralDirectoryOffset =
        readUnsigned32(archiveBytes, endRecordOffset + endRecordDirectoryStartOffset);

    // Step 3. Walk the index, one entry at a time.
    int walkOffset = static_cast<int>(centralDirectoryOffset);
    for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        if (walkOffset < 0 || walkOffset + directoryEntryFixedSize > archiveBytes.size()
                || readUnsigned32(archiveBytes, walkOffset) != centralDirectoryEntrySignature) {
            lastErrorDescription = QStringLiteral("The archive's index is damaged.");
            qWarning().noquote() << QStringLiteral(
                "ZipArchiveReader::readCentralDirectory: index entry %1 of %2 in \"%3\""
                " is missing or has the wrong signature — the index is damaged")
                    .arg(entryIndex + 1).arg(entryCount).arg(archivePath);
            return false;
        }

        ArchiveEntry entry;
        entry.compressionMethod =
            readUnsigned16(archiveBytes, walkOffset + directoryEntryMethodOffset);
        entry.compressedSize =
            readUnsigned32(archiveBytes, walkOffset + directoryEntryCompressedSizeOffset);
        entry.uncompressedSize =
            readUnsigned32(archiveBytes, walkOffset + directoryEntryUncompressedSizeOffset);
        entry.localHeaderOffset =
            readUnsigned32(archiveBytes, walkOffset + directoryEntryLocalHeaderOffset);

        const quint16 nameLength =
            readUnsigned16(archiveBytes, walkOffset + directoryEntryNameLengthOffset);
        const quint16 extraLength =
            readUnsigned16(archiveBytes, walkOffset + directoryEntryExtraLengthOffset);
        const quint16 commentLength =
            readUnsigned16(archiveBytes, walkOffset + directoryEntryCommentLengthOffset);

        // The name sits right after the fixed part of the entry.
        entry.entryName = QString::fromUtf8(
            archiveBytes.mid(walkOffset + directoryEntryFixedSize, nameLength));
        archiveEntries.append(entry);

        walkOffset += directoryEntryFixedSize + nameLength + extraLength + commentLength;
    }
    return true;
}

QStringList ZipArchiveReader::entryNames() const
{
    QStringList names;
    names.reserve(archiveEntries.size());
    for (const ArchiveEntry &entry : archiveEntries) {
        names.append(entry.entryName);
    }
    return names;
}

bool ZipArchiveReader::containsEntry(const QString &entryName) const
{
    for (const ArchiveEntry &entry : archiveEntries) {
        if (entry.entryName == entryName) {
            return true;
        }
    }
    return false;
}

QByteArray ZipArchiveReader::readEntry(const QString &entryName, bool &found) const
{
    found = false;
    for (const ArchiveEntry &entry : archiveEntries) {
        if (entry.entryName != entryName) {
            continue;
        }

        // The central directory says WHERE the entry is; the local header at
        // that spot says how much padding sits in front of the actual bytes.
        // Both have to be read. The name and extra-field lengths in the local
        // header often differ from the ones in the index. Using the index's
        // lengths here would start reading in the wrong place.
        const int localOffset = static_cast<int>(entry.localHeaderOffset);
        if (localOffset < 0 || localOffset + localHeaderFixedSize > archiveBytes.size()
                || readUnsigned32(archiveBytes, localOffset) != localFileHeaderSignature) {
            qWarning().noquote() << QStringLiteral(
                "ZipArchiveReader::readEntry: no local header for \"%1\" in \"%2\""
                " — the archive is damaged").arg(entryName, archivePath);
            return QByteArray();
        }
        const quint16 localNameLength =
            readUnsigned16(archiveBytes, localOffset + localHeaderNameLengthOffset);
        const quint16 localExtraLength =
            readUnsigned16(archiveBytes, localOffset + localHeaderExtraLengthOffset);

        // The file's bytes start after the local header's fixed part, its
        // name and its extra field. The index says how many bytes there are.
        const int dataOffset =
            localOffset + localHeaderFixedSize + localNameLength + localExtraLength;
        const QByteArray storedBytes =
            archiveBytes.mid(dataOffset, static_cast<int>(entry.compressedSize));

        if (entry.compressionMethod == storedCompressionMethod) {
            found = true;
            return storedBytes; // small files are often not compressed at all
        }
        if (entry.compressionMethod == deflateCompressionMethod) {
            const DeflateDecompressor decompressor;
            bool inflateFailed = false;
            const QByteArray inflated = decompressor.inflate(storedBytes, inflateFailed);
            // Partial output still beats nothing: a document that lost its
            // last paragraph is more use than a blank one.
            if (inflateFailed) {
                qWarning().noquote() << QStringLiteral(
                    "ZipArchiveReader::readEntry: \"%1\" in \"%2\" did not fully"
                    " decompress — kept the first %3 of %4 bytes")
                        .arg(entryName, archivePath)
                        .arg(inflated.size())
                        .arg(entry.uncompressedSize);
            }
            found = !inflated.isEmpty();
            return inflated;
        }
        qWarning().noquote() << QStringLiteral(
            "ZipArchiveReader::readEntry: \"%1\" in \"%2\" uses compression method %3"
            " — only stored (0) and deflate (8) can be read")
                .arg(entryName, archivePath).arg(entry.compressionMethod);
        return QByteArray(); // a compression method Word does not produce
    }
    return QByteArray();
}
