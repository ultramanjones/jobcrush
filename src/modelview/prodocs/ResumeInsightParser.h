#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "../../model/EducationRecord.h"
#include "../../model/WorkExperience.h"

// ParsedResumeInsights
//
// What one document appeared to say. Every entry is a READING, not a fact —
// nothing here is true until the user has looked at it.
struct ParsedResumeInsights {
    QList<WorkExperience> workExperiences;
    QList<EducationRecord> educationRecords;
    QStringList skillTerms;
};

// ResumeInsightParser
//
// Reads jobs, schooling and skills out of resume text.
//
// Local and deterministic, like everything else that runs on every document:
// it costs nothing, works with no brain configured, works offline, and re-runs
// instantly when a document changes. Moonlight is offered afterwards as a
// second opinion the user asks for — never as the thing standing between them
// and their own resume.
//
// THE RULE THIS FILE IS BUILT AROUND: never invent. Prime directive two says
// the user's documents are the truth and employment history is never
// fabricated, and a parser is exactly where that gets violated by accident.
// So every field is either copied from the document or left empty, dates stay
// as the words the resume used, and each entry carries the line it came from
// so a wrong reading can be traced instead of merely doubted.
//
// A calculation with no state, so it stays a plain ModelView class.
class ResumeInsightParser {
public:
    ParsedResumeInsights parseResumeText(const QString &rawResumeText) const;
};
