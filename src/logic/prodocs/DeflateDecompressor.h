#pragma once

#include <QByteArray>

// DeflateDecompressor
//
// Undoes DEFLATE compression (RFC 1951) — the only compression a .docx uses.
//
// Written by hand rather than pulled from a library because Qt has no
// public inflate. An outside library for one small, well-defined algorithm
// would cost more than it saves: another thing to build on Windows, another
// licence to follow, another version to keep up with.
//
// DEFLATE is a finished specification from 1996. It will not change, so
// writing it out once is cheaper than depending on someone else's copy.
//
// The code is dense, so the .cpp explains every step.
class DeflateDecompressor {
public:
    // Returns the decompressed bytes. On damaged input it returns what it
    // managed to decode, sets failed, and logs where it stopped. A document
    // missing its end is still worth showing.
    QByteArray inflate(const QByteArray &compressedBytes, bool &failed) const;
};
