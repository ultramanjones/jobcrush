#pragma once

#include <QString>

// PipelineStage
//
// The stages a targeted job moves through on the board.
// Stored in the database as human-readable text (see pipelineStageToStorageText),
// because a person inspecting the database should be able to read their own data.
enum class PipelineStage {
    Saved,      // targeted (crushed) but not yet applied
    Applied,    // application packet has been sent by the human
    Interview,  // at least one interview scheduled or completed
    Offer,      // an offer is on the table
    Closed      // finished: rejected, withdrawn, or declined
};

// Converts a PipelineStage to the exact text stored in the database.
inline QString pipelineStageToStorageText(PipelineStage stage)
{
    switch (stage) {
    case PipelineStage::Saved:     return QStringLiteral("saved");
    case PipelineStage::Applied:   return QStringLiteral("applied");
    case PipelineStage::Interview: return QStringLiteral("interview");
    case PipelineStage::Offer:     return QStringLiteral("offer");
    case PipelineStage::Closed:    return QStringLiteral("closed");
    }
    return QStringLiteral("saved"); // unreachable, but the compiler deserves certainty
}

// Converts stored text back to a PipelineStage.
// Unknown text falls back to Saved rather than crashing — a damaged row should
// never take the whole board down.
inline PipelineStage pipelineStageFromStorageText(const QString &storageText)
{
    if (storageText == QStringLiteral("applied"))   return PipelineStage::Applied;
    if (storageText == QStringLiteral("interview")) return PipelineStage::Interview;
    if (storageText == QStringLiteral("offer"))     return PipelineStage::Offer;
    if (storageText == QStringLiteral("closed"))    return PipelineStage::Closed;
    return PipelineStage::Saved;
}
