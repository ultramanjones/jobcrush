#include "ZipArchiveReader.h"

#include <QFile>

#include "DeflateDecompressor.h"

namespace {

// ZIP stores its numbers little-endian, unsigned, at fixed offsets. These
// three readers keep that fact in one place instead of scattered arithmetic.
quint16 readUnsigned16(const QByteArray &bytes, int offset)
{
    if (offset < 0 || offset + 1 >= bytes.size()) {
        return 0;
    }
    return static_cast<quint16>(static_cast<unsigned char>(bytes.at(offset)))
         | (static_cast<quint16>(static_cast<unsigned char>(bytes.at(offset + 1))) << 8);
}

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

// The signatures that mark each ZIP structure.
constexpr quint32 endOfCentralDirectorySignature = 0x06054b50;
constexpr quint32 centralDirectoryEntrySignature = 0x02014b50;
constexpr quint32 localFileHeaderSignature       = 0x04034b50;

// A ZIP file is read BACKWARDS: the index lives at the end, after a comment
// of unknown length, so the only way in is to scan back for its signature.
constexpr int longestPossibleArchiveComment = 65535;

} // namespace

ZipArchiveReader::ZipArchiveReader(const QString &archiveFilePath)
    : archivePath(archiveFilePath)
{
    QFile archiveFile(archiveFilePath);
    if (!archiveFile.open(QIODevice::ReadOnly)) {
        lastErrorDescription = QStringLiteral("Couldn't open the file to read it.");
        return;
    }
    archiveBytes = archiveFile.readAll();
    archiveFile.close();

    archiveIsUsable = readCentralDirectory();
}

bool ZipArchiveReader::readCentralDirectory()
{
    // Scan backwards for the end-of-central-directory record.
    const int searchFloor =
        qMax(0, archiveBytes.size() - longestPossibleArchiveComment - 22);
    int endRecordOffset = -1;
    for (int offset = archiveBytes.size() - 22; offset >= searchFloor; --offset) {
        if (readUnsigned32(archiveBytes, offset) == endOfCentralDirectorySignature) {
            endRecordOffset = offset;
            break;
        }
    }
    if (endRecordOffset < 0) {
        lastErrorDescription = QStringLiteral("That file isn't a ZIP archive.");
        return false;
    }

    const quint16 entryCount = readUnsigned16(archiveBytes, endRecordOffset + 10);
    const quint32 centralDirectoryOffset = readUnsigned32(archiveBytes, endRecordOffset + 16);

    int walkOffset = static_cast<int>(centralDirectoryOffset);
    for (int entryIndex = 0; entryIndex < entryCount; ++entryIndex) {
        if (walkOffset < 0 || walkOffset + 46 > archiveBytes.size()
                || readUnsigned32(archiveBytes, walkOffset) != centralDirectoryEntrySignature) {
            lastErrorDescription = QStringLiteral("The archive's index is damaged.");
            return false;
        }

        ArchiveEntry entry;
        entry.compressionMethod  = readUnsigned16(archiveBytes, walkOffset + 10);
        entry.compressedSize     = readUnsigned32(archiveBytes, walkOffset + 20);
        entry.uncompressedSize   = readUnsigned32(archiveBytes, walkOffset + 24);
        entry.localHeaderOffset  = readUnsigned32(archiveBytes, walkOffset + 42);

        const quint16 nameLength    = readUnsigned16(archiveBytes, walkOffset + 28);
        const quint16 extraLength   = readUnsigned16(archiveBytes, walkOffset + 30);
        const quint16 commentLength = readUnsigned16(archiveBytes, walkOffset + 32);

        entry.entryName = QString::fromUtf8(archiveBytes.mid(walkOffset + 46, nameLength));
        archiveEntries.append(entry);

        walkOffset += 46 + nameLength + extraLength + commentLength;
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
        // Both have to be read — the name and extra-field lengths routinely
        // differ between the two, which is a classic way to read garbage.
        const int localOffset = static_cast<int>(entry.localHeaderOffset);
        if (localOffset < 0 || localOffset + 30 > archiveBytes.size()
                || readUnsigned32(archiveBytes, localOffset) != localFileHeaderSignature) {
            return QByteArray();
        }
        const quint16 localNameLength  = readUnsigned16(archiveBytes, localOffset + 26);
        const quint16 localExtraLength = readUnsigned16(archiveBytes, localOffset + 28);

        const int dataOffset = localOffset + 30 + localNameLength + localExtraLength;
        const QByteArray storedBytes =
            archiveBytes.mid(dataOffset, static_cast<int>(entry.compressedSize));

        if (entry.compressionMethod == 0) {
            found = true;
            return storedBytes; // small files are often not compressed at all
        }
        if (entry.compressionMethod == 8) {
            const DeflateDecompressor decompressor;
            bool inflateFailed = false;
            const QByteArray inflated = decompressor.inflate(storedBytes, inflateFailed);
            // Partial output still beats nothing: a document that lost its
            // last paragraph is more use than a blank one.
            found = !inflated.isEmpty();
            return inflated;
        }
        return QByteArray(); // a compression method Word does not produce
    }
    return QByteArray();
}
