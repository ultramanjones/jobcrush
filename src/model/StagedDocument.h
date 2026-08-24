#pragma once

#include <QDateTime>
#include <QString>

// StagedDocumentKind
//
// What a piece of the packet is. Stored as readable text, like every other
// enum in this app, so a person can open the database file and read it.
enum class StagedDocumentKind {
    CoverLetter,      // the letter — always the first page of an export
    TailoredResume,   // their resume, aimed at this one job
    Checklist,        // what this employer asks for, and what is done
    PostingSummary,   // the posting read back in plain words
    FitNote,          // why the fit score is what it is
    FollowUpNote,     // what to say, and when, after sending
    Other
};

inline QString stagedDocumentKindToStorageText(StagedDocumentKind documentKind)
{
    switch (documentKind) {
    case StagedDocumentKind::CoverLetter:    return QStringLiteral("coverLetter");
    case StagedDocumentKind::TailoredResume: return QStringLiteral("tailoredResume");
    case StagedDocumentKind::Checklist:      return QStringLiteral("checklist");
    case StagedDocumentKind::PostingSummary: return QStringLiteral("postingSummary");
    case StagedDocumentKind::FitNote:        return QStringLiteral("fitNote");
    case StagedDocumentKind::FollowUpNote:   return QStringLiteral("followUpNote");
    case StagedDocumentKind::Other:          return QStringLiteral("other");
    }
    return QStringLiteral("other");
}

inline StagedDocumentKind stagedDocumentKindFromStorageText(const QString &storageText)
{
    if (storageText == QStringLiteral("coverLetter"))    return StagedDocumentKind::CoverLetter;
    if (storageText == QStringLiteral("tailoredResume")) return StagedDocumentKind::TailoredResume;
    if (storageText == QStringLiteral("checklist"))      return StagedDocumentKind::Checklist;
    if (storageText == QStringLiteral("postingSummary")) return StagedDocumentKind::PostingSummary;
    if (storageText == QStringLiteral("fitNote"))        return StagedDocumentKind::FitNote;
    if (storageText == QStringLiteral("followUpNote"))   return StagedDocumentKind::FollowUpNote;
    return StagedDocumentKind::Other;
}

// The order the packet is assembled in, lowest number first.
//
// The employer opens one file. The cover letter is first, then the resume,
// then everything else. This is a fact about the packet, not about the
// screen, so it lives here instead of in the view.
inline int packetOrderOf(StagedDocumentKind documentKind)
{
    switch (documentKind) {
    case StagedDocumentKind::CoverLetter:    return 0;
    case StagedDocumentKind::TailoredResume: return 1;
    case StagedDocumentKind::Checklist:      return 2;
    case StagedDocumentKind::PostingSummary: return 3;
    case StagedDocumentKind::FitNote:        return 4;
    case StagedDocumentKind::FollowUpNote:   return 5;
    case StagedDocumentKind::Other:          return 6;
    }
    return 6;
}

// True if this piece gets sent to the employer. False if it is working
// material the user keeps. Do not send the employer the note explaining why
// the app scored the job 62%.
inline bool belongsInTheSentPacket(StagedDocumentKind documentKind)
{
    return documentKind == StagedDocumentKind::CoverLetter
        || documentKind == StagedDocumentKind::TailoredResume;
}

// StagedDocument
//
// One piece of one application packet.
//
// Always stored as markdown. Never shown to the user as markdown. Markdown is
// a good working format: it diffs, edits and converts cleanly. It is not
// something to hand a person who wants to print their cover letter. Exports
// always go out as Word or PDF. See PacketExporter.
//
// Nothing here is sent anywhere by the app. The user reads it, fixes it,
// approves it, exports it, and sends it themselves.
struct StagedDocument {
    qint64 stagedDocumentId = 0;
    qint64 jobApplicationId = 0;      // the campaign this belongs to
    StagedDocumentKind documentKind = StagedDocumentKind::Other;

    QString titleText;                // what the user sees at the top of the card
    QString markdownText;             // the master copy — internal format only

    // Where this came from, and whether anyone has looked at it. The UI needs
    // to show at a glance which pieces are unread drafts.
    bool wasWrittenByBrain = false;
    bool wasEditedByUser = false;
    bool isApprovedByUser = false;

    QDateTime createdTimestamp;
    QDateTime lastEditedTimestamp;
};
