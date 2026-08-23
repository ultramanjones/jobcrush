#pragma once

#include <QString>

// DroppedFileClassifier
//
// Decides what a dropped file IS — resume, transcript, certification,
// reference, cover letter, photo — from its name and its words.
//
// Local and deterministic, on purpose. This runs on every single drop, and
// spending the user's AI credits to answer "is this a resume?" would be the
// wrong shape: it costs money per file, needs a configured brain, and fails
// offline. The heuristic is right the overwhelming majority of the time, the
// user can correct it in one click, and Moonlight is available to look
// harder at a single document when they ask her to.
//
// A calculation with no state, so it stays a plain ModelView class.
class DroppedFileClassifier {
public:
    // documentText may be empty (an image, or a format not readable yet); the
    // filename alone still says a great deal.
    QString classifyDocument(const QString &filePath, const QString &documentText) const;
};
