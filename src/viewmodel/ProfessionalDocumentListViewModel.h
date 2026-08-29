#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

#include "../model/ProfessionalDocument.h"
#include "../modelview/exporting/DocumentConverter.h"

class AppPreferences;
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

    // What just happened on this page, in one human sentence: a file that was
    // dropped, or a copy that was saved out. The notice shows it verbatim,
    // including the parts that are bad news.
    Q_PROPERTY(QString lastProDocsOutcomeText READ lastProDocsOutcomeText
                   NOTIFY lastProDocsOutcomeChanged)

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
                                      AppPreferences &preferences,
                                      const QString &applicationDataFolderPath,
                                      QObject *parent = nullptr);

    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const override;
    QVariant data(const QModelIndex &modelIndex, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int rowCountForProperty() const;
    QString lastProDocsOutcomeText() const;

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

    // --- Saving a copy in another format ---

    // The formats offered, in the order they are shown, and the one-word name
    // that fits on a button.
    Q_INVOKABLE QStringList selectableConversionFormats() const;
    Q_INVOKABLE QString conversionFormatButtonName(const QString &format) const;

    // Writes this document out as Word, PDF or plain text, into the folder
    // Job Crush writes to. Reports what happened through
    // lastProDocsOutcomeText, including the full path when it worked, because
    // a file the user cannot find was not delivered.
    Q_INVOKABLE void saveCopyOfDocumentAt(int rowIndex, const QString &format);

    // The sentence shown next to the buttons, before anything is pressed. The
    // page design does not survive a conversion and the user hears that from
    // us first, not from the file.
    Q_INVOKABLE QString warningAboutConversion() const;

signals:
    void documentsChanged();
    void lastProDocsOutcomeChanged();

private:
    void reloadDocuments();

    void reportOutcome(const QString &outcomeText);

    ProfessionalDocumentRepository &repository;
    ProDocsIntake &proDocsIntake;
    AppPreferences &appPreferences;
    const QString applicationDataFolderPath;
    DocumentConverter documentConverter;
    QList<ProfessionalDocument> loadedDocuments;
    QString storedLastProDocsOutcomeText;
};
