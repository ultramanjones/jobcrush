#include "DeflateDecompressor.h"

#include <QList>

namespace {

// --- Reading bits, least significant first --------------------------------
//
// DEFLATE packs its bits backwards relative to how people write numbers: the
// first bit of the stream is the LOW bit of the first byte. Every read here
// goes through this one class so that convention is stated once.
class BitReader {
public:
    explicit BitReader(const QByteArray &sourceBytes)
        : bytes(sourceBytes)
    {
    }

    bool ranOutOfInput() const { return exhausted; }

    // One bit.
    int readBit()
    {
        if (bitsHeld == 0) {
            if (nextByteIndex >= bytes.size()) {
                exhausted = true;
                return 0;
            }
            bitBuffer = static_cast<unsigned char>(bytes.at(nextByteIndex));
            ++nextByteIndex;
            bitsHeld = 8;
        }
        const int bit = bitBuffer & 1;
        bitBuffer >>= 1;
        --bitsHeld;
        return bit;
    }

    // A little-endian run of bits, as a number.
    int readBits(int bitCount)
    {
        int value = 0;
        for (int bitPosition = 0; bitPosition < bitCount; ++bitPosition) {
            value |= readBit() << bitPosition;
        }
        return value;
    }

    // Stored blocks are byte-aligned, so any part-used byte is dropped.
    void alignToByteBoundary()
    {
        bitsHeld = 0;
        bitBuffer = 0;
    }

    int bytesRemaining() const { return bytes.size() - nextByteIndex; }

    QByteArray readRawBytes(int byteCount)
    {
        const int available = qMin(byteCount, bytes.size() - nextByteIndex);
        const QByteArray raw = bytes.mid(nextByteIndex, available);
        nextByteIndex += available;
        if (available < byteCount) {
            exhausted = true;
        }
        return raw;
    }

private:
    const QByteArray &bytes;
    int nextByteIndex = 0;
    unsigned int bitBuffer = 0;
    int bitsHeld = 0;
    bool exhausted = false;
};

// --- Canonical Huffman ----------------------------------------------------
//
// DEFLATE never ships the codes themselves, only how LONG each symbol's code
// is. That is enough, because the codes are assigned in a fixed order:
// shortest first, and alphabetically by symbol within a length. Rebuilding
// the table from the lengths is what this does.
class HuffmanTable {
public:
    void buildFromCodeLengths(const QList<int> &codeLengths)
    {
        symbolCount = codeLengths.size();
        countOfEachLength = QList<int>(maximumCodeLength + 1, 0);
        for (int codeLength : codeLengths) {
            ++countOfEachLength[codeLength];
        }
        countOfEachLength[0] = 0; // length 0 means "symbol unused"

        // Where each length's block of symbols starts in the sorted list.
        QList<int> firstSymbolIndexOfLength(maximumCodeLength + 1, 0);
        int runningTotal = 0;
        for (int length = 1; length <= maximumCodeLength; ++length) {
            firstSymbolIndexOfLength[length] = runningTotal;
            runningTotal += countOfEachLength[length];
        }

        symbolsSortedByCode = QList<int>(runningTotal, 0);
        QList<int> nextSlotOfLength = firstSymbolIndexOfLength;
        for (int symbol = 0; symbol < codeLengths.size(); ++symbol) {
            const int length = codeLengths.at(symbol);
            if (length > 0) {
                symbolsSortedByCode[nextSlotOfLength[length]] = symbol;
                ++nextSlotOfLength[length];
            }
        }
    }

    // Walks the stream one bit at a time until the accumulated bits identify
    // exactly one symbol. Returns -1 when the code is invalid.
    int decodeNextSymbol(BitReader &bitReader) const
    {
        int code = 0;
        int firstCodeOfLength = 0;
        int firstSymbolIndex = 0;

        for (int length = 1; length <= maximumCodeLength; ++length) {
            code |= bitReader.readBit();
            const int codesOfThisLength = countOfEachLength[length];

            // Is the code we have so far inside this length's range?
            if (code - firstCodeOfLength < codesOfThisLength) {
                const int symbolIndex = firstSymbolIndex + (code - firstCodeOfLength);
                if (symbolIndex < 0 || symbolIndex >= symbolsSortedByCode.size()) {
                    return -1;
                }
                return symbolsSortedByCode.at(symbolIndex);
            }

            firstSymbolIndex += codesOfThisLength;
            firstCodeOfLength = (firstCodeOfLength + codesOfThisLength) << 1;
            code <<= 1;

            if (bitReader.ranOutOfInput()) {
                return -1;
            }
        }
        return -1;
    }

    bool isEmpty() const { return symbolsSortedByCode.isEmpty(); }

private:
    static constexpr int maximumCodeLength = 15; // the specification's ceiling

    int symbolCount = 0;
    QList<int> countOfEachLength;
    QList<int> symbolsSortedByCode;
};

// --- The specification's fixed tables -------------------------------------
//
// Match lengths 257-285 and distances 0-29 each carry a base value plus a
// number of extra bits read straight from the stream. These are copied from
// RFC 1951 and are not open to interpretation.
const int matchLengthBases[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43,
    51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
const int matchLengthExtraBits[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3,
    3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
const int matchDistanceBases[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
const int matchDistanceExtraBits[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
    8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

// The order the code-length code lengths arrive in for a dynamic block —
// deliberately scrambled by the specification so the commonest come first.
const int codeLengthAlphabetOrder[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

void buildFixedTables(HuffmanTable &literalTable, HuffmanTable &distanceTable)
{
    // Fixed blocks use a table every DEFLATE implementation already knows.
    QList<int> literalCodeLengths(288, 8);
    for (int symbol = 144; symbol <= 255; ++symbol) literalCodeLengths[symbol] = 9;
    for (int symbol = 256; symbol <= 279; ++symbol) literalCodeLengths[symbol] = 7;
    literalTable.buildFromCodeLengths(literalCodeLengths);

    distanceTable.buildFromCodeLengths(QList<int>(30, 5));
}

bool readDynamicTables(BitReader &bitReader,
                       HuffmanTable &literalTable,
                       HuffmanTable &distanceTable)
{
    const int literalCodeCount = bitReader.readBits(5) + 257;
    const int distanceCodeCount = bitReader.readBits(5) + 1;
    const int codeLengthCodeCount = bitReader.readBits(4) + 4;

    if (literalCodeCount > 288 || distanceCodeCount > 32) {
        return false;
    }

    // First, the tiny table that describes the other two tables.
    QList<int> codeLengthCodeLengths(19, 0);
    for (int index = 0; index < codeLengthCodeCount; ++index) {
        codeLengthCodeLengths[codeLengthAlphabetOrder[index]] = bitReader.readBits(3);
    }
    HuffmanTable codeLengthTable;
    codeLengthTable.buildFromCodeLengths(codeLengthCodeLengths);

    // Then the literal and distance lengths, run-length encoded together.
    QList<int> allCodeLengths;
    allCodeLengths.reserve(literalCodeCount + distanceCodeCount);

    while (allCodeLengths.size() < literalCodeCount + distanceCodeCount) {
        const int symbol = codeLengthTable.decodeNextSymbol(bitReader);
        if (symbol < 0) {
            return false;
        }

        if (symbol < 16) {
            allCodeLengths.append(symbol);
        } else if (symbol == 16) {
            // Repeat the previous length 3-6 times.
            if (allCodeLengths.isEmpty()) {
                return false;
            }
            const int previousLength = allCodeLengths.last();
            const int repeatCount = 3 + bitReader.readBits(2);
            for (int repeat = 0; repeat < repeatCount; ++repeat) {
                allCodeLengths.append(previousLength);
            }
        } else if (symbol == 17) {
            const int repeatCount = 3 + bitReader.readBits(3);   // 3-10 zeroes
            for (int repeat = 0; repeat < repeatCount; ++repeat) {
                allCodeLengths.append(0);
            }
        } else {
            const int repeatCount = 11 + bitReader.readBits(7);  // 11-138 zeroes
            for (int repeat = 0; repeat < repeatCount; ++repeat) {
                allCodeLengths.append(0);
            }
        }

        if (bitReader.ranOutOfInput()) {
            return false;
        }
    }

    if (allCodeLengths.size() != literalCodeCount + distanceCodeCount) {
        return false; // a repeat ran off the end of the table
    }

    literalTable.buildFromCodeLengths(allCodeLengths.mid(0, literalCodeCount));
    distanceTable.buildFromCodeLengths(allCodeLengths.mid(literalCodeCount));
    return true;
}

} // namespace

QByteArray DeflateDecompressor::inflate(const QByteArray &compressedBytes, bool &failed) const
{
    failed = false;
    QByteArray output;
    BitReader bitReader(compressedBytes);

    bool isFinalBlock = false;
    while (!isFinalBlock) {
        isFinalBlock = bitReader.readBit() == 1;
        const int blockType = bitReader.readBits(2);

        if (bitReader.ranOutOfInput()) {
            failed = true;
            break;
        }

        if (blockType == 0) {
            // Stored: the bytes are simply there, byte-aligned, with their
            // length written twice — once plain, once inverted as a check.
            bitReader.alignToByteBoundary();
            const int storedLength = bitReader.readBits(16);
            bitReader.readBits(16); // the one's complement copy
            output.append(bitReader.readRawBytes(storedLength));
            if (bitReader.ranOutOfInput()) {
                failed = true;
                break;
            }
            continue;
        }

        HuffmanTable literalTable;
        HuffmanTable distanceTable;

        if (blockType == 1) {
            buildFixedTables(literalTable, distanceTable);
        } else if (blockType == 2) {
            if (!readDynamicTables(bitReader, literalTable, distanceTable)) {
                failed = true;
                break;
            }
        } else {
            failed = true; // block type 3 is reserved and never valid
            break;
        }

        // The block body: literals go straight out, and a length/distance
        // pair copies bytes already written — which is the whole trick.
        while (true) {
            const int symbol = literalTable.decodeNextSymbol(bitReader);
            if (symbol < 0) {
                failed = true;
                break;
            }
            if (symbol == 256) {
                break; // end of block
            }
            if (symbol < 256) {
                output.append(static_cast<char>(symbol));
                continue;
            }

            const int lengthIndex = symbol - 257;
            if (lengthIndex < 0 || lengthIndex >= 29) {
                failed = true;
                break;
            }
            const int copyLength = matchLengthBases[lengthIndex]
                                   + bitReader.readBits(matchLengthExtraBits[lengthIndex]);

            const int distanceSymbol = distanceTable.decodeNextSymbol(bitReader);
            if (distanceSymbol < 0 || distanceSymbol >= 30) {
                failed = true;
                break;
            }
            const int copyDistance =
                matchDistanceBases[distanceSymbol]
                + bitReader.readBits(matchDistanceExtraBits[distanceSymbol]);

            if (copyDistance > output.size()) {
                failed = true; // pointing back before the start of the data
                break;
            }

            // Copied ONE BYTE AT A TIME on purpose: the run may overlap
            // itself, which is how DEFLATE expresses a repeating pattern.
            // Copying the block wholesale would get repeats wrong.
            const int copyStart = output.size() - copyDistance;
            for (int copied = 0; copied < copyLength; ++copied) {
                output.append(output.at(copyStart + copied));
            }

            if (bitReader.ranOutOfInput()) {
                failed = true;
                break;
            }
        }

        if (failed) {
            break;
        }
    }

    return output;
}
