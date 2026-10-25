#pragma once

#include <QList>
#include <QString>

#include "ProfessionalDocument.h"

class JobCrushDatabase;

// ProfessionalDocumentRepository
//
// Reads and writes ProDocs rows. Callers pass and get back plain
// ProfessionalDocument records. Only this class writes SQL for them.
class ProfessionalDocumentRepository {
public:
    explicit ProfessionalDocumentRepository(JobCrushDatabase &database);

    // Saves a document and fills in its id. False only on a real failure.
    bool insertProfessionalDocument(ProfessionalDocument &professionalDocument);

    // Everything ProDocs holds, newest first.
    QList<ProfessionalDocument> loadAllProfessionalDocuments();

    // Only one kind — the resume, the transcripts, and so on. Newest first.
    QList<ProfessionalDocument> loadProfessionalDocumentsOfKind(const QString &documentKind);

    // Records that jobs and schooling have been read out of this document,
    // so it is never read a second time behind the user's back.
    bool markDocumentAsRead(qint64 professionalDocumentId);

    // Puts every document back to "not read yet", so the next startup reads
    // them all again. Used when the READER has improved — the documents have
    // not changed, but what Job Crush can get out of them has.
    bool markEveryDocumentUnread();

    // The user correcting a guess. Classification is a heuristic, so being
    // wrong is expected and putting it right must cost one click.
    bool updateDocumentKind(qint64 professionalDocumentId, const QString &documentKind);

    // Forgets the record. The stored copy is removed by the layer above,
    // which is the only one that knows where files live.
    bool removeProfessionalDocument(qint64 professionalDocumentId);

    // Every word from every readable document, joined with blank lines. The
    // search profile adds terms from this text, so job scores improve as
    // soon as documents are added.
    QString allExtractedTextJoined();

    // Why the last write failed.
    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
