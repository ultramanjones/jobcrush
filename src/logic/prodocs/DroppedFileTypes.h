#pragma once

#include <QFileInfo>
#include <QString>
#include <QStringList>

// DroppedFileTypes
//
// What Job Crush will and will not take when something lands on the window.
//
// Filtered, but only lightly. Documents get read; images are kept for a
// profile picture or a letterhead later even though there is no text in them
// to read. Everything else is refused OUT LOUD — a basket that silently
// swallows a video and files it as "other" is worse than one that says no,
// because the user walks away believing something happened.
namespace DroppedFileTypes {

inline QStringList readableDocumentExtensions()
{
    return { QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("markdown"),
             QStringLiteral("pdf"), QStringLiteral("docx"), QStringLiteral("rtf"),
             QStringLiteral("odt") };
}

inline QStringList imageExtensions()
{
    return { QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
             QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("bmp") };
}

inline QString loweredExtensionOf(const QString &filePath)
{
    return QFileInfo(filePath).suffix().toLower();
}

inline bool isReadableDocument(const QString &filePath)
{
    return readableDocumentExtensions().contains(loweredExtensionOf(filePath));
}

inline bool isImage(const QString &filePath)
{
    return imageExtensions().contains(loweredExtensionOf(filePath));
}

inline bool isAccepted(const QString &filePath)
{
    return isReadableDocument(filePath) || isImage(filePath);
}

// Why a file was turned away, written for the person who dropped it.
//
// Never a verdict about them — a statement about what Job Crush can do. And
// NEVER a bare refusal: every branch here ends with the next thing to try.
// "No" on its own leaves someone stuck holding a document they need read,
// with no idea whether the problem is their file, their computer or them.
inline QString reasonFileCannotBeAccepted(const QString &filePath)
{
    if (isAccepted(filePath)) {
        return QString();
    }
    const QString extension = loweredExtensionOf(filePath);

    if (extension.isEmpty()) {
        return QStringLiteral("That file has no extension on the end of its name, so "
                              "Job Crush can't tell what's inside it. Rename it with "
                              ".pdf, .docx or .txt on the end — whichever it actually "
                              "is — and drop it again.");
    }
    if (extension == QStringLiteral("doc")) {
        return QStringLiteral("That's the older Word format. Open it in Word, choose "
                              "Save As, and pick .docx or PDF — then drop that in and "
                              "it'll come straight through.");
    }
    if (extension == QStringLiteral("zip") || extension == QStringLiteral("rar")
            || extension == QStringLiteral("7z")) {
        return QStringLiteral("That's a compressed folder, so Job Crush can't see what's "
                              "inside it. Unzip it first, then drop the documents "
                              "themselves in — you can drop several at once.");
    }
    if (extension == QStringLiteral("pages")) {
        return QStringLiteral("That's an Apple Pages file. In Pages choose File ▸ Export "
                              "To ▸ PDF, then drop the PDF in.");
    }
    return QStringLiteral("Job Crush takes documents and pictures, and it doesn't know "
                          "what to do with a .%1 file. If there's a document inside it "
                          "or alongside it, drop that one in instead — PDF, Word, plain "
                          "text and pictures all work.").arg(extension);
}

} // namespace DroppedFileTypes
