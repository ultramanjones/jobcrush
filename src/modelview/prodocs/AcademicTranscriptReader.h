#pragma once

#include <QString>

#include "ResumeInsightParser.h"

// AcademicTranscriptReader
//
// Reads an academic transcript.
//
// Why this is separate from the resume parser:
//
// 1. The resume parser works by finding section headings. A transcript has no
//    EDUCATION heading, because the whole page is the education section. So
//    the resume parser never started recording and returned nothing for all
//    three test transcripts.
//
// 2. A transcript is one school. Reading it with resume rules gives one row
//    per course line - forty rows of "CHEM 1113" for the user to delete one at
//    a time. So course rows are skipped on purpose. The course text is still
//    stored on the document, so the AI can read it.
//
// The output is one record: the school, the credential, the subject, and the
// first and last term.
//
// One record per transcript. Transcripts often list transfer credit from other
// schools. Reading those out is a later change; making up a school from a
// footer would be worse than missing it.
//
// No state, so it stays a plain ModelView class.
class AcademicTranscriptReader {
public:
    // Returns education records only. An empty list means this did not look
    // like a transcript. The caller then falls back to the resume parser.
    ParsedResumeInsights parseTranscriptText(const QString &transcriptText) const;
};
