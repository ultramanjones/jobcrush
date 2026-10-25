#include "DeflateDecompressor.h"

#include <QDebug>
#include <QList>

// How DEFLATE works, in short (RFC 1951)
//
// The compressed data is a series of blocks. Each block starts with a
// three-bit header:
//   bit 1      1 when this is the last block, 0 when more follow
//   bits 2-3   the block type: 0 stored, 1 fixed Huffman, 2 dynamic Huffman
//
// A stored block holds plain bytes with no compression.
//
// The other two kinds hold a stream of Huffman codes. Each code stands for a
// symbol from one of two lists, called alphabets:
//   - The literal/length alphabet, symbols 0 to 287.
//       0-255    a literal byte. Write it to the output as it is.
//       256      end of this block.
//       257-285  a match length. A distance code follows it. Together they
//                say "go back DISTANCE bytes in what you have written so
//                far, and copy LENGTH bytes from there to the end."
//   - The distance alphabet, symbols 0 to 29, used only after a length.
//
// A fixed block uses Huffman codes written into the specification. A dynamic
// block sends its own codes first, then the data.
//
// Matches are how DEFLATE saves space. A word seen once can be written again
// later as a short length and distance instead of all its letters.

namespace {

// --- Block types (the two-bit type field in each block header) -----------
constexpr int storedBlockType = 0;
constexpr int fixedHuffmanBlockType = 1;
constexpr int dynamicHuffmanBlockType = 2;
// Type 3 is reserved by the specification and is always an error.

// --- Special symbols in the literal/length alphabet -----------------------
constexpr int endOfBlockSymbol = 256;
constexpr int firstMatchLengthSymbol = 257;

// --- Alphabet sizes, from the specification -------------------------------
constexpr int matchLengthSymbolCount = 29;      // symbols 257 to 285
constexpr int matchDistanceSymbolCount = 30;    // distance symbols 0 to 29
constexpr int largestLiteralAlphabetSize = 288; // a dynamic block may not say more
constexpr int largestDistanceAlphabetSize = 32; // a dynamic block may not say more
constexpr int codeLengthAlphabetSize = 19;      // code length symbols 0 to 18

// --- Symbols in the code length alphabet (dynamic blocks only) ------------
// 0 to 15 are a code length, written as it is. The other three repeat.
constexpr int repeatPreviousLengthSymbol = 16;  // the last length, 3 to 6 times
constexpr int repeatShortRunOfZeroesSymbol = 17; // a zero, 3 to 10 times
// Symbol 18 repeats a zero 11 to 138 times. It is the only symbol left, so
// the code reaches it through the final "else".

// --- Reading bits, least significant first --------------------------------
//
// DEFLATE packs its bits in the opposite order from how people write
// numbers. The first bit of the stream is the LOW bit of the first byte,
// then the next-lowest bit, and so on. When a byte runs out, the next byte
// starts again at its low bit. Every read here goes through this one class,
// so that rule is written down in one place.
class BitReader {
public:
    explicit BitReader(const QByteArray &sourceBytes)
        : bytes(sourceBytes)
    {
    }

    // True once a read asked for more bits or bytes than the input holds.
    bool ranOutOfInput() const { return exhausted; }

    // Reads one bit.
    //
    // bitBuffer holds the bits of the current byte that are not used yet.
    // The next bit to hand out is always bit 0 of bitBuffer. After handing
    // it out, the buffer shifts right by one so the next bit moves into
    // bit 0. When bitsHeld reaches zero, the next byte is loaded.
    //
    // Past the end of the input this returns 0 and sets the exhausted flag.
    // The callers check that flag. They do not trust the zero.
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

    // Reads bitCount bits and builds a number from them.
    //
    // The first bit read is the number's lowest bit (bit 0), the second is
    // bit 1, and so on. This is how DEFLATE stores every field EXCEPT the
    // Huffman codes themselves. Those go the other way, and
    // HuffmanTable::decodeNextSymbol reads them one bit at a time instead.
    int readBits(int bitCount)
    {
        int value = 0;
        for (int bitPosition = 0; bitPosition < bitCount; ++bitPosition) {
            value |= readBit() << bitPosition;
        }
        return value;
    }

    // A stored block starts on a whole byte. Any bits left in the current
    // byte are padding, so they are thrown away.
    void alignToByteBoundary()
    {
        bitsHeld = 0;
        bitBuffer = 0;
    }

    int bytesRemaining() const { return bytes.size() - nextByteIndex; }

    // Copies whole bytes straight from the input. Only valid right after
    // alignToByteBoundary. If the input has fewer bytes than asked for, this
    // returns what there is and sets the exhausted flag.
    QByteArray readRawBytes(int byteCount)
    {
        const int availableByteCount = qMin(byteCount, bytes.size() - nextByteIndex);
        const QByteArray rawBytes = bytes.mid(nextByteIndex, availableByteCount);
        nextByteIndex += availableByteCount;
        if (availableByteCount < byteCount) {
            exhausted = true;
        }
        return rawBytes;
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
// A Huffman code gives short bit patterns to common symbols and long ones
// to rare symbols. DEFLATE never sends the bit patterns themselves. It
// sends only how LONG each symbol's pattern is, from 1 to 15 bits. A length
// of 0 means the symbol is not used.
//
// The lengths are enough, because the patterns are handed out by a fixed
// rule (the "canonical" rule):
//   1. Shorter patterns come before longer ones.
//   2. Among patterns of the same length, lower symbols get lower patterns.
//   3. Patterns of one length are consecutive numbers. The first pattern of
//      the next length is (first pattern of this length + how many patterns
//      this length has), doubled. The doubling adds a 0 bit on the right,
//      which makes the pattern one bit longer.
//
// Example. Symbols A, B, C, D with lengths 2, 1, 3, 3:
//   B (length 1) gets 0
//   A (length 2) gets 10
//   C (length 3) gets 110
//   D (length 3) gets 111
//
// This table keeps two things that are enough to decode with:
//   countOfEachLength    how many symbols use each length
//   symbolsSortedByCode  every used symbol, in the order of rule 1 and 2
// It never stores the patterns. decodeNextSymbol works them out as it reads.
class HuffmanTable {
public:
    // Builds the table from one code length per symbol. codeLengths[s] is
    // the length of symbol s.
    void buildFromCodeLengths(const QList<int> &codeLengths)
    {
        symbolCount = codeLengths.size();

        // Step 1. Count how many symbols use each length.
        countOfEachLength = QList<int>(maximumCodeLength + 1, 0);
        for (int codeLength : codeLengths) {
            ++countOfEachLength[codeLength];
        }
        countOfEachLength[0] = 0; // length 0 means "symbol unused"

        // Step 2. Work out where each length's group starts in the sorted
        // list. Length 1 starts at 0. Each later length starts right after
        // all the shorter ones.
        QList<int> firstSymbolIndexOfLength(maximumCodeLength + 1, 0);
        int runningTotal = 0;
        for (int length = 1; length <= maximumCodeLength; ++length) {
            firstSymbolIndexOfLength[length] = runningTotal;
            runningTotal += countOfEachLength[length];
        }

        // Step 3. Put each used symbol in the next free slot of its length's
        // group. Walking the symbols from lowest to highest puts them in
        // symbol order inside each group, which is rule 2.
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

    // Reads one Huffman pattern from the stream and returns its symbol.
    // Returns -1 when the bits match no symbol, or the input ran out.
    //
    // Huffman patterns are stored with their FIRST bit first (the highest
    // bit of the pattern comes out of the stream first). So each new bit is
    // added on the right of the pattern read so far.
    //
    // The loop tries length 1, then 2, and so on up to 15. For each length:
    //   code               the bits read so far, as a number
    //   firstCodeOfLength  the lowest pattern of this length (rule 3 above)
    //   codesOfThisLength  how many patterns this length has
    //   firstSymbolIndex   where this length's symbols start in the sorted list
    //
    // If code falls inside [firstCodeOfLength, firstCodeOfLength + count),
    // the pattern is complete. Its place in the group is
    // (code - firstCodeOfLength), so the symbol sits at
    // firstSymbolIndex + (code - firstCodeOfLength) in the sorted list.
    //
    // If not, the pattern is longer. Move past this length's group in the
    // sorted list, work out the first pattern of the next length by rule 3,
    // and shift code left one place to make room for the next bit.
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

            // Not yet. Step to the next length.
            firstSymbolIndex += codesOfThisLength;
            firstCodeOfLength = (firstCodeOfLength + codesOfThisLength) << 1;
            code <<= 1;

            if (bitReader.ranOutOfInput()) {
                return -1;
            }
        }
        return -1; // 15 bits read and still no match: the data is damaged
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
// A length symbol (257-285) or a distance symbol (0-29) does not give the
// number directly. It gives a base value and a count of extra bits. The
// extra bits are read from the stream right after the symbol, as a plain
// number (lowest bit first), and added to the base.
//
// Example: length symbol 266 is index 9 here. Its base is 13 and it has 1
// extra bit. So 266 means a length of 13 or 14, and the extra bit says which.
//
// These tables are copied from RFC 1951, section 3.2.5.
const int matchLengthBases[matchLengthSymbolCount] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43,
    51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
const int matchLengthExtraBits[matchLengthSymbolCount] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3,
    3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
const int matchDistanceBases[matchDistanceSymbolCount] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
const int matchDistanceExtraBits[matchDistanceSymbolCount] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
    8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

// A dynamic block sends the lengths for the code length alphabet in this
// order, not in order 0 to 18. The specification puts the most-used symbols
// first, so a block can stop early and leave the rare ones out.
const int codeLengthAlphabetOrder[codeLengthAlphabetSize] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

// Builds the two tables for a fixed Huffman block. They come from RFC 1951,
// section 3.2.6, and are the same in every DEFLATE stream:
//   literal/length symbols   0-143  8 bits
//                          144-255  9 bits
//                          256-279  7 bits
//                          280-287  8 bits
//   distance symbols 0-29           5 bits each
void buildFixedTables(HuffmanTable &literalTable, HuffmanTable &distanceTable)
{
    QList<int> literalCodeLengths(largestLiteralAlphabetSize, 8);
    for (int symbol = 144; symbol <= 255; ++symbol) literalCodeLengths[symbol] = 9;
    for (int symbol = 256; symbol <= 279; ++symbol) literalCodeLengths[symbol] = 7;
    literalTable.buildFromCodeLengths(literalCodeLengths);

    distanceTable.buildFromCodeLengths(QList<int>(matchDistanceSymbolCount, 5));
}

// Reads the Huffman tables at the start of a dynamic block.
//
// A dynamic block describes its tables in three layers:
//
//   1. Three counts:
//        HLIT   5 bits  + 257  how many literal/length lengths follow
//        HDIST  5 bits  + 1    how many distance lengths follow
//        HCLEN  4 bits  + 4    how many code length lengths follow
//
//   2. HCLEN lengths, 3 bits each, for the small "code length" alphabet
//      (symbols 0-18). They arrive in codeLengthAlphabetOrder. Any symbol
//      not given a length here has length 0. These build a small Huffman
//      table used only for step 3.
//
//   3. HLIT + HDIST lengths for the two real tables, one after the other,
//      written with the small table from step 2. Each symbol read is:
//        0-15  that length
//        16    repeat the previous length 3-6 times (2 extra bits, + 3)
//        17    repeat a zero 3-10 times               (3 extra bits, + 3)
//        18    repeat a zero 11-138 times             (7 extra bits, + 11)
//      A repeat may run from the literal lengths into the distance lengths.
//      That is why the two lists are read as one and split afterwards.
//
// Returns false when the counts are out of range, a code does not decode,
// a repeat has nothing to repeat, or the input runs out.
bool readDynamicTables(BitReader &bitReader,
                       HuffmanTable &literalTable,
                       HuffmanTable &distanceTable)
{
    // Layer 1: the three counts.
    const int literalCodeCount = bitReader.readBits(5) + 257;
    const int distanceCodeCount = bitReader.readBits(5) + 1;
    const int codeLengthCodeCount = bitReader.readBits(4) + 4;

    if (literalCodeCount > largestLiteralAlphabetSize
            || distanceCodeCount > largestDistanceAlphabetSize) {
        return false;
    }

    // Layer 2: the small table that describes the other two tables.
    QList<int> codeLengthCodeLengths(codeLengthAlphabetSize, 0);
    for (int orderPosition = 0; orderPosition < codeLengthCodeCount; ++orderPosition) {
        codeLengthCodeLengths[codeLengthAlphabetOrder[orderPosition]] = bitReader.readBits(3);
    }
    HuffmanTable codeLengthTable;
    codeLengthTable.buildFromCodeLengths(codeLengthCodeLengths);

    // Layer 3: the literal and distance lengths, run-length encoded together.
    QList<int> allCodeLengths;
    allCodeLengths.reserve(literalCodeCount + distanceCodeCount);

    while (allCodeLengths.size() < literalCodeCount + distanceCodeCount) {
        const int symbol = codeLengthTable.decodeNextSymbol(bitReader);
        if (symbol < 0) {
            return false;
        }

        if (symbol < repeatPreviousLengthSymbol) {
            // 0 to 15: the length itself.
            allCodeLengths.append(symbol);
        } else if (symbol == repeatPreviousLengthSymbol) {
            // Repeat the previous length 3-6 times. There must be one.
            if (allCodeLengths.isEmpty()) {
                return false;
            }
            const int previousLength = allCodeLengths.last();
            const int repeatCount = 3 + bitReader.readBits(2);
            for (int repeat = 0; repeat < repeatCount; ++repeat) {
                allCodeLengths.append(previousLength);
            }
        } else if (symbol == repeatShortRunOfZeroesSymbol) {
            const int repeatCount = 3 + bitReader.readBits(3);   // 3-10 zeroes
            for (int repeat = 0; repeat < repeatCount; ++repeat) {
                allCodeLengths.append(0);
            }
        } else {
            // Symbol 18, the only one left.
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

    // Split the one list back into the two tables.
    literalTable.buildFromCodeLengths(allCodeLengths.mid(0, literalCodeCount));
    distanceTable.buildFromCodeLengths(allCodeLengths.mid(literalCodeCount));
    return true;
}

} // namespace

// Decodes blocks until the one marked "last" is done, or something goes
// wrong. Every failure sets failed and stops. The bytes decoded before the
// failure are still returned.
QByteArray DeflateDecompressor::inflate(const QByteArray &compressedBytes, bool &failed) const
{
    failed = false;
    QByteArray output;
    BitReader bitReader(compressedBytes);

    bool isFinalBlock = false;
    while (!isFinalBlock) {
        // The block header: one "last block" bit, then two bits of type.
        isFinalBlock = bitReader.readBit() == 1;
        const int blockType = bitReader.readBits(2);

        if (bitReader.ranOutOfInput()) {
            failed = true;
            qWarning().noquote() << QStringLiteral(
                "DeflateDecompressor::inflate: block header cut off after %1 output bytes"
                " — the compressed data ended early").arg(output.size());
            break;
        }

        if (blockType == storedBlockType) {
            // Stored: skip to the next whole byte. Then come LEN (16 bits),
            // NLEN (16 bits, LEN with every bit flipped), and LEN raw bytes.
            // NLEN is meant as a check. This reader skips it without
            // checking it.
            bitReader.alignToByteBoundary();
            const int storedLength = bitReader.readBits(16);
            bitReader.readBits(16); // NLEN, the one's complement copy, unchecked
            output.append(bitReader.readRawBytes(storedLength));
            if (bitReader.ranOutOfInput()) {
                failed = true;
                qWarning().noquote() << QStringLiteral(
                    "DeflateDecompressor::inflate: stored block of %1 bytes cut off"
                    " — the compressed data ended early").arg(storedLength);
                break;
            }
            continue;
        }

        HuffmanTable literalTable;
        HuffmanTable distanceTable;

        if (blockType == fixedHuffmanBlockType) {
            buildFixedTables(literalTable, distanceTable);
        } else if (blockType == dynamicHuffmanBlockType) {
            if (!readDynamicTables(bitReader, literalTable, distanceTable)) {
                failed = true;
                qWarning().noquote() << QStringLiteral(
                    "DeflateDecompressor::inflate: could not read the tables of a dynamic"
                    " block after %1 output bytes — the table data is damaged or cut off")
                        .arg(output.size());
                break;
            }
        } else {
            failed = true; // block type 3 is reserved and never valid
            qWarning().noquote() << QStringLiteral(
                "DeflateDecompressor::inflate: block type 3 after %1 output bytes"
                " — that type is reserved, so the data is damaged").arg(output.size());
            break;
        }

        // The block body. Read one symbol at a time:
        //   0-255    a literal byte. Write it out.
        //   256      the end of the block.
        //   257-285  a match. Work out the length (base + extra bits), then
        //            read a distance symbol and work out the distance the
        //            same way. Then copy LENGTH bytes, starting DISTANCE
        //            bytes back from the end of the output.
        while (true) {
            const int symbol = literalTable.decodeNextSymbol(bitReader);
            if (symbol < 0) {
                failed = true;
                break;
            }
            if (symbol == endOfBlockSymbol) {
                break;
            }
            if (symbol < endOfBlockSymbol) {
                output.append(static_cast<char>(symbol));
                continue;
            }

            const int lengthIndex = symbol - firstMatchLengthSymbol;
            if (lengthIndex < 0 || lengthIndex >= matchLengthSymbolCount) {
                failed = true; // symbols 286 and 287 exist but are never valid
                break;
            }
            const int copyLength = matchLengthBases[lengthIndex]
                                   + bitReader.readBits(matchLengthExtraBits[lengthIndex]);

            const int distanceSymbol = distanceTable.decodeNextSymbol(bitReader);
            if (distanceSymbol < 0 || distanceSymbol >= matchDistanceSymbolCount) {
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

            // Copied ONE BYTE AT A TIME on purpose. The length may be longer
            // than the distance. Then the copy reads bytes it has just
            // written, and that is how DEFLATE writes a repeating pattern.
            // Example: output ends in "ab", distance 2, length 6. Byte by
            // byte this writes "ababab". A single block copy of 6 bytes from
            // 2 back would read past the end of what exists.
            const int copyStart = output.size() - copyDistance;
            for (int copiedCount = 0; copiedCount < copyLength; ++copiedCount) {
                output.append(output.at(copyStart + copiedCount));
            }

            if (bitReader.ranOutOfInput()) {
                failed = true;
                break;
            }
        }

        if (failed) {
            qWarning().noquote() << QStringLiteral(
                "DeflateDecompressor::inflate: stopped inside a block after %1 output bytes"
                " — a code did not decode, pointed outside the data, or the data ended"
                " early").arg(output.size());
            break;
        }
    }

    return output;
}
