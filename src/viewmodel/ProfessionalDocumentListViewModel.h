#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

#include "../model/ProfessionalDocument.h"

class ProDocsIntake;
class ProfessionalDocumentRepository;

// ProfessionalDocumentListViewModel
//
// Serves the ProDocs Documents tab its rows, and carries the drop basket's
// state — what was just dropped and what Job Crush made of it.
//
// Translation and organization only: copying, reading and classifying all
// happen below, in ProDocsIntake (ModelView).
class ProfessionalDocumentListViewModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int documentCount READ rowCountForProperty NOTIFY documentsChanged)

    // What just happened to a dropped file, in one human sentence. The basket
    // shows it verbatim, including the parts that are bad news.
    Q_PROPERTY(QString lastDropOutcomeText READ lastDropOutcomeText
                   NOTIFY lastDropOutcomeChanged)

public:
    enum DocumentRole {
        DisplayNameRole = Qt::UserRole + 1,
        DocumentKindRole,          // storage name: "resume", "photo"…
        DocumentKindLabelRole,     // what a human reads: "Resume", "Photo"
        ImportedWhenTextRole,
        FileSizeTextRole,
        ExtractionNoteRole,        // empty when the text came out fine
        HasReadableTextRole,
        WordCountTextRole
    };

    ProfessionalDocumentListViewModel(ProfessionalDocumentRepository &documentRepository,
                                      ProDocsIntake &intake,
                                      QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountForProperty() const;
    QString lastDropOutcomeText() const;

    // The drop basket hands over what landed on the window.
    Q_INVOKABLE void acceptDroppedFiles(const QStringList &droppedFilePaths);

    // Classification is a guess, so correcting it costs one click.
    Q_INVOKABLE QStringList selectableDocumentKinds() const;
    Q_INVOKABLE QString documentKindLabel(const QString &documentKind) const;
    Q_INVOKABLE void setDocumentKindAt(int rowIndex, const QString &documentKind);

    // Removes the record and Job Crush's own copy. The user's original is
    // never touched — it was never ours.
    Q_INVOKABLE void removeDocumentAt(int rowIndex);

    // Opens Job Crush's copy in whatever the system uses for that file.
    Q_INVOKABLE void openDocumentAt(int rowIndex) const;

    // Reads every stored document again. Anything the user confirmed or typed
    // themselves survives it.
    Q_INVOKABLE void rereadEveryDocument();

signals:
    void documentsChanged();
    void lastDropOutcomeChanged();

private:
    void reloadDocuments();

    ProfessionalDocumentRepository &repository;
    ProDocsIntake &proDocsIntake;
    QList<ProfessionalDocument> loadedDocuments;
    QString storedLastDropOutcomeText;
};
