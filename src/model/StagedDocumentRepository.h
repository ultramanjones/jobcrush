#pragma once

#include <QList>
#include <QString>

#include "StagedDocument.h"

class JobCrushDatabase;

// StagedDocumentRepository
//
// The model-layer gateway for application packets. One row per piece of one
// packet. A packet is every row with the same jobApplicationId. There is no
// separate packet table because a packet has no identity of its own.
class StagedDocumentRepository {
public:
    explicit StagedDocumentRepository(JobCrushDatabase &database);

    bool insertStagedDocument(StagedDocument &stagedDocument);
    bool updateStagedDocument(const StagedDocument &stagedDocument);
    bool removeStagedDocument(qint64 stagedDocumentId);

    // One campaign's packet, in packet order (letter first). An empty list is
    // a normal answer, not an error.
    QList<StagedDocument> loadPacketForApplication(qint64 jobApplicationId);

    // Every row, for screens that only need counts.
    QList<StagedDocument> loadEveryStagedDocument();

    // How many pieces a campaign has, and how many the user has approved. The
    // Staging list shows "2 of 3 checked off". Counting here instead of in the
    // view keeps one answer.
    int countForApplication(qint64 jobApplicationId);
    int approvedCountForApplication(qint64 jobApplicationId);

    // Replaces the piece the AI just rewrote, or inserts it if there was none.
    // Running the same task twice must not leave two cover letters in the
    // packet.
    //
    // A piece the user has edited is never overwritten. Returns false only on
    // a database error. Refusing to overwrite the user's own writing is not an
    // error, and is reported through wasRefusedBecauseUserEdited.
    bool replaceGeneratedDocument(StagedDocument &stagedDocument,
                                  bool &wasRefusedBecauseUserEdited);

    // Deletes a whole packet, for when its campaign is deleted.
    bool removePacketForApplication(qint64 jobApplicationId);

    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
