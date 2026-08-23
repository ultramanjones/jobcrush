#pragma once

#include <QByteArray>

// DeflateDecompressor
//
// Undoes DEFLATE compression (RFC 1951) — the only compression a .docx uses.
//
// Written by hand rather than pulled from a library because Qt exposes no
// public inflate, and taking on an external dependency for one well-defined
// algorithm would cost more than it saves: another thing to build on Windows,
// another licence to honour, another version to chase.
//
// DEFLATE is a closed, finished specification from 1996. It will not change
// under us, which makes it one of the few places where writing it out is
// genuinely cheaper than depending on someone else's copy.
//
// Dense by nature, so it is commented at every step.
class DeflateDecompressor {
public:
    // Returns the decompressed bytes. On malformed input it returns whatever
    // it managed and sets failed — a truncated document is still worth
    // showing, and silence would be worse.
    QByteArray inflate(const QByteArray &compressedBytes, bool &failed) const;
};
