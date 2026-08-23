#pragma once

#include <QString>

// EducationRecord
//
// One school, course of study or credential, as Job Crush understood it from
// the user's documents. Dates are text for the same reason as WorkExperience.
struct EducationRecord {
    qint64 educationRecordId = 0;
    QString schoolName;
    QString credentialText;      // "B.S.", "Associate of Applied Science", "Certificate"
    QString fieldOfStudyText;
    QString startDateText;
    QString endDateText;

    qint64 sourceDocumentId = 0;
    QString sourceLineText;

    bool isConfirmedByUser = false;
};
