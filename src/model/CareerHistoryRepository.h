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

    QString lastErrorText() const;

private:
    JobCrushDatabase &jobCrushDatabase;
    QString lastErrorDescription;
};
