#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "../../model/EducationRecord.h"
#include "../../model/WorkExperience.h"

class AiBrain;
class ProDocsIntake;

// MoonlightDocumentCleanup
//
// "Moonlight, clean this up!" on a ProDocs document.
//
// Job Crush's own reader pulls jobs and schooling out of a resume with plain
// rules, and an unusual layout can leave it with a mess: two schools in one
// box, a job title read as an employer. This hands the document's text to the
// connected AI brain and asks for the jobs and the schools in a fixed JSON
// shape. The answer replaces what the built-in reader got out of that one
// document. Anything the user typed into is kept.
//
// It spends the user's AI credits, so it only ever runs when they press the
// button. Nothing here runs on its own.
class MoonlightDocumentCleanup : public QObject {
    Q_OBJECT
public:
    MoonlightDocumentCleanup(AiBrain &aiBrain, ProDocsIntake &intake,
                             QObject *parent = nullptr);

    // True when a brain is connected and can be asked.
    bool aBrainIsAvailable() const;

    // True while Moonlight is reading a document.
    bool cleanupIsRunning() const;

    // Starts reading one document. documentName is only for the sentence
    // the user reads.
    void cleanUpDocument(qint64 professionalDocumentId, const QString &documentName,
                         const QString &documentText);

    // "Want it to look right? Moonlight can lay it out properly."
    //
    // The plain Word copy keeps the words and loses the page design: a
    // two-column resume comes out as one column of lines. This asks the
    // brain to lay the same words out with headings, dates and bullets in
    // the right places, and writes that as a Word file in destinationFolder.
    // It is a clean new layout, not a copy of the original page.
    void layOutDocument(const QString &documentName, const QString &documentText,
                        const QString &destinationFolderPath);

    // Reads the brain's answer. Public so it can be tested without a brain.
    static bool readAnswer(const QString &replyText,
                           QList<WorkExperience> &workExperiences,
                           QList<EducationRecord> &educationRecords,
                           QString &whyNot);

signals:
    // Running started or stopped. outcomeText says what happened.
    void cleanupStatusChanged(const QString &outcomeText);

private:
    AiBrain &connectedBrain;
    ProDocsIntake &proDocsIntake;
    bool isRunning = false;
};
