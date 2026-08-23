#pragma once

#include <QString>

// WorkExperience
//
// One job the user has held, as Job Crush understood it from their documents.
//
// Dates are TEXT, not dates. Resumes say "2016 – 2019", "Jan 2016 to Present",
// "Summer 2018" — forcing those into a QDate would mean inventing precision
// nobody wrote down, and inventing detail about somebody's employment history
// is the one thing this app must never do. They are shown back exactly as
// they were written.
struct WorkExperience {
    qint64 workExperienceId = 0;
    QString employerName;
    QString roleTitle;
    QString startDateText;
    QString endDateText;
    QString summaryText;

    // Which ProDocs document this came from, and the exact line it was read
    // from. Shown to the user so a wrong guess is traceable rather than
    // mysterious — "where did it get THAT?" should always have an answer.
    qint64 sourceDocumentId = 0;
    QString sourceLineText;

    // True once the user has looked at this entry and let it stand. Until
    // then it is Job Crush's reading, not a fact, and the UI says so.
    bool isConfirmedByUser = false;
};
