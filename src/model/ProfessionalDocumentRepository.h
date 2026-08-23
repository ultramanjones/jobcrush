#pragma once

#include <QList>
#include <QString>

#include "ProfessionalDocument.h"

class JobCrushDatabase;

// ProfessionalDocumentRepository
//
// The model-layer gateway for ProDocs rows. Everything above speaks in
// ProfessionalDocument structs; only this class speaks SQL.
class ProfessionalDocumentRepository {
public:
    explicit ProfessionalDocumentRepository(JobCrushDatabase &database);

    // Saves a document and fills in its id. False only on a real failure.
    bool insertProfessionalDocument(ProfessionalDocument &professionalDocument);

    // Everything ProDocs holds, newest first.
    QList<ProfessionalDocument> loadAllProfessionalDocuments();

    // Just one kind — the resume, the transcripts, and so on.
    QList<ProfessionalDocument> loadProfessionalDocumentsOfKind(const QString &documentKind);

    // The user correcting a guess. Classification is a heuristic, so being
    // wrong is expected and putting it right must cost one click.
    bool updateDocumentKind(qint64 professionalDocumentId, const QString &documentKind);

    // Forgets the record. The stored copy is removed by the layer above,
    // which is the only one that knows where files live.
    bool removeProfessionalDocument(qint64 professionalDocumentId);

    // Every word from every readable document, joined. This is the seed the
    // rest of Job Crush grows from — the search profile tops itself up from
    // here, so scoring gets sharper the moment documents arrive.
    QString allExtractedTextJoined();

    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
