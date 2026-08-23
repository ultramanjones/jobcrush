#pragma once

#include <QList>
#include <QString>

#include "EducationRecord.h"
#include "WorkExperience.h"

class JobCrushDatabase;

// CareerHistoryRepository
//
// The model-layer gateway for the two things a resume is really made of: the
// jobs someone has held and the schooling they have done.
//
// One repository rather than two, because they are never wanted apart — every
// screen and every calculation that needs one needs the other, and splitting
// them would be separation of concerns turning into confetti.
class CareerHistoryRepository {
public:
    explicit CareerHistoryRepository(JobCrushDatabase &database);

    // --- Work experience --------------------------------------------------
    bool insertWorkExperience(WorkExperience &workExperience);
    bool updateWorkExperience(const WorkExperience &workExperience);
    bool removeWorkExperience(qint64 workExperienceId);
    QList<WorkExperience> loadAllWorkExperiences();

    // --- Education --------------------------------------------------------
    bool insertEducationRecord(EducationRecord &educationRecord);
    bool updateEducationRecord(const EducationRecord &educationRecord);
    bool removeEducationRecord(qint64 educationRecordId);
    QList<EducationRecord> loadAllEducationRecords();

    // Clears everything Job Crush READ from documents while keeping anything
    // the user confirmed or typed themselves. Re-reading documents must never
    // throw away a correction someone made by hand.
    bool removeUnconfirmedEntries();

    // --- Not reading the same line twice ----------------------------------
    //
    // An entry that came out of a document is identified by WHICH document and
    // WHICH line of it. Re-reading that document produces the same line again,
    // and without these checks it produces a second copy of the entry.
    //
    // That is exactly what went wrong: "read my documents again" cleared the
    // unconfirmed entries first, so anything the user had CONFIRMED survived
    // the clear — and was then created all over again by the re-read. Every
    // confirmed entry grew a twin, every time.
    //
    // Hand-typed entries carry no source line, so they are never matched here
    // and never suppressed.
    //
    // The entry's OWN identity is part of the match, not just the line it came
    // from: one line of a resume can legitimately name two schools ("Tulsa
    // Community College · 2000 – 2001    Auburn University · 1993 – 1994"),
    // and keying on the line alone would let the first one through and
    // silently swallow the second.
    bool workExperienceAlreadyRecorded(const WorkExperience &workExperience);
    bool educationRecordAlreadyRecorded(const EducationRecord &educationRecord);

    // Throws away every entry the READER produced that nobody has typed into
    // since. Used when the reader itself has improved: its old output is fair
    // game, the user's own words never are.
    bool removeEntriesTheReaderProducedAndNobodyTouched();

    // Collapses duplicates already sitting in the database, keeping the one
    // the user confirmed where there is one, and the oldest otherwise. Runs at
    // startup so a database that got into this state before the fix heals
    // itself rather than needing the user to tidy up after us.
    // Returns how many rows it removed, or -1 on failure.
    int removeDuplicateEntries();

    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
