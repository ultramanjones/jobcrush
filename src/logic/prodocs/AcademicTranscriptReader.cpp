#include "AcademicTranscriptReader.h"

#include <QRegularExpression>
#include <QStringList>

#include "InstitutionWords.h"
#include "PlainTextNormalizer.h"

namespace {

// How far down the page a school name can appear and still be the letterhead.
// Transcripts put the school at the top. A school named forty lines down is a
// transfer credit line or a footer.
constexpr int deepestLineThatCouldBeTheLetterhead = 18;

// A school name read off the letterhead is at most this long. A longer
// "name" means the line held more than the name.
constexpr int longestSchoolName = 80;

// A field of study is at most this long. Longer means the parse went wrong.
constexpr int longestFieldOfStudy = 60;

// A school name with no school word in it ("Le Cordon Bleu") is between
// these lengths. Shorter is a fragment. Longer is a sentence.
constexpr int shortestUnlabelledSchoolName = 6;
constexpr int longestUnlabelledSchoolName = 60;

// A year from 1900 to 2099: "19" or "20", then two digits.
const QString yearPattern = QStringLiteral("(?:19|20)\\d{2}");

// Is this line a course row? Getting this right is what keeps the output to
// one record instead of forty.
bool looksLikeACourseRow(const QString &line)
{
    // "CHEM 1113", "MATH-2413", "ENG101", "BIOL 2124L"
    // At the start of the line: 2 to 4 capital letters, an optional space,
    // an optional hyphen, an optional space, 3 or 4 digits, and an optional
    // capital letter. Then a word boundary, so "ENG1013X2" does not match.
    static const QRegularExpression courseCodePattern(
        QStringLiteral("^[A-Z]{2,4}\\s?-?\\s?\\d{3,4}[A-Z]?\\b"));
    if (courseCodePattern.match(line).hasMatch()) {
        return true;
    }

    // Credit hours then a letter grade: "3.00 A", "4.00 B+".
    // A number like 3.00 (one digit, a point, two digits), then anything,
    // then a single letter A to F with an optional + or -.
    static const QRegularExpression creditsThenGradePattern(
        QStringLiteral("\\b\\d\\.\\d{2}\\b.*\\b[A-F][+-]?\\b"));
    if (creditsThenGradePattern.match(line).hasMatch()) {
        return true;
    }

    // The totals at the bottom of each term. A line that starts with one of
    // these words, in any case.
    static const QRegularExpression tallyPattern(
        QStringLiteral("^\\s*(?:GPA|CUM|TERM|SEM|TOTALS?|EARNED|ATTEMPTED|QUALITY\\s+POINTS"
                       "|TRANSFER\\s+(?:CREDIT|TOTALS)|INSTITUTIONAL|CUMULATIVE)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return tallyPattern.match(line).hasMatch();
}

// "Degree Awarded: Bachelor of Science", "DEGREE CONFERRED 05/2013".
//
// Two steps:
//   1. Is this an award line? It must hold "awarded", "conferred",
//      "granted" or "earned" as a whole word. A "degree", "credential",
//      "certificate" or "diploma" in front of it is allowed but not needed.
//   2. If so, find the credential's name on the line. The pattern is the
//      same list the resume parser uses (see credentialPhraseIn in
//      ResumeInsightParser.cpp, which explains the order of the
//      alternatives). This copy has no "ged".
// Returns the credential as the line wrote it, or empty.
QString credentialOnAwardLine(const QString &line)
{
    static const QRegularExpression awardLinePattern(
        QStringLiteral("\\b(?:degree|credential|certificate|diploma)?\\s*"
                       "(?:awarded|conferred|granted|earned)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (!awardLinePattern.match(line).hasMatch()) {
        return QString();
    }

    static const QRegularExpression credentialPattern(
        QStringLiteral("\\b(?:"
                       "(?:bachelor|master|associate|doctor)(?:'s)?\\s+of\\s+"
                       "(?:applied\\s+science|business\\s+administration|fine\\s+arts"
                       "|science|arts|engineering|education|nursing|philosophy)"
                       "|ph\\.?\\s?d\\.?|doctorate"
                       "|bachelor(?:'s)?|master(?:'s)?|associate(?:'s)?"
                       "|m\\.?b\\.?a\\.?|b\\.?s\\.?c?\\.?|b\\.?a\\.?"
                       "|m\\.?s\\.?|m\\.?a\\.?|a\\.?a\\.?s?\\.?"
                       "|certificate|certification|diploma"
                       ")\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch credentialMatch = credentialPattern.match(line);
    if (!credentialMatch.hasMatch()) {
        // The line says something was awarded but does not name it. Leave the
        // credential empty rather than guessing.
        return QString();
    }

    // A word boundary can't sit after a full stop, so "B.S." matches as
    // "B.S". Take the full stop back.
    int matchEnd = credentialMatch.capturedEnd(0);
    if (matchEnd < line.length() && line.at(matchEnd) == QLatin1Char('.')) {
        ++matchEnd;
    }
    return line.mid(credentialMatch.capturedStart(0),
                    matchEnd - credentialMatch.capturedStart(0)).trimmed();
}

// "Major: Computer Science", "Program of Study - Nursing", "Plan: Accounting".
//
// Steps:
//   1. Find a label word ("major", "program", "program of study", "plan",
//      "field of study", "degree program", "curriculum", "concentration"),
//      then ":" or a dash. Everything after that is the first guess.
//   2. Cut at the next label on the same line, or at a gap of two or more
//      spaces.
//   3. Trim punctuation off both ends.
//   4. Throw it away if it has no run of 3 letters, or is too long.
QString subjectOnALabelledLine(const QString &line)
{
    static const QRegularExpression subjectLinePattern(
        QStringLiteral("\\b(?:major|program(?:\\s+of\\s+study)?|plan|field\\s+of\\s+study"
                       "|degree\\s+program|curriculum|concentration)\\b\\s*[:\\-–—]\\s*(.+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch subjectMatch = subjectLinePattern.match(line);
    if (!subjectMatch.hasMatch()) {
        return QString();
    }

    QString subject = subjectMatch.captured(1).trimmed();

    // Transcripts put several labels on one line: "Major: Biology  Minor:
    // Chemistry  College: Arts & Sciences". Stop at the next label.
    static const QRegularExpression nextLabelPattern(
        QStringLiteral("\\s{2,}|\\s+(?:minor|college|school|degree|plan|catalog|status)\\b\\s*[:\\-]"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch nextLabel = nextLabelPattern.match(subject);
    if (nextLabel.hasMatch()) {
        subject = subject.left(nextLabel.capturedStart()).trimmed();
    }

    static const QRegularExpression danglingEdgePattern(
        QStringLiteral("^[\\s,;:|\\-–—]+|[\\s,;:|\\-–—]+$"));
    subject.remove(danglingEdgePattern);

    // A major that is a number or a course code means the parse went wrong.
    static const QRegularExpression hasLettersPattern(QStringLiteral("[A-Za-z]{3,}"));
    if (!hasLettersPattern.match(subject).hasMatch()
            || subject.length() > longestFieldOfStudy) {
        return QString();
    }
    return subject;
}

// Pulls the school name out of a letterhead line, dropping the address that
// often follows it.
QString schoolNameOutOf(const QString &line)
{
    QString schoolName = line;

    // In "TULSA COMMUNITY COLLEGE  ·  OFFICIAL TRANSCRIPT", everything after
    // the separator is the document title, not the school name. A separator
    // is "|", "·" or "•" with spaces around it, three or more spaces, or a
    // tab. The cut is kept only if the first piece still names a school.
    static const QRegularExpression separatorPattern(
        QStringLiteral("\\s+[|·•]\\s+|\\s{3,}|\\t+"));
    const QRegularExpressionMatch separator = separatorPattern.match(schoolName);
    if (separator.hasMatch()) {
        const QString firstPiece = schoolName.left(separator.capturedStart()).trimmed();
        if (InstitutionWords::namesASchool(firstPiece)) {
            schoolName = firstPiece;
        }
    }

    // A trailing address: ", Tulsa, OK 74135". A comma, a city made of
    // letters, spaces and dots, a comma, a two-letter state, and a ZIP code
    // of 5 digits with an optional "-1234", at the end of the line.
    static const QRegularExpression trailingAddressPattern(
        QStringLiteral(",\\s*[A-Za-z .]+,\\s*[A-Z]{2}\\s*\\d{5}(?:-\\d{4})?\\s*$"));
    schoolName.remove(trailingAddressPattern);

    static const QRegularExpression danglingEdgePattern(
        QStringLiteral("^[\\s,;:|\\-–—•·]+|[\\s,;:|\\-–—•·]+$"));
    schoolName.remove(danglingEdgePattern);

    return schoolName.simplified();
}

// The document talking about itself. "OFFICIAL TRANSCRIPT" is not where
// anybody studied, and on a trade-school record — which has no "university" or
// "college" in its name to disqualify it — it is the very first thing that
// would otherwise be mistaken for one.
bool lineIsTheDocumentTalkingAboutItself(const QString &line)
{
    static const QRegularExpression titlePattern(
        QStringLiteral("^\\s*(?:official|unofficial|student|academic|complete|issued|page"
                       "|this\\s+is|office\\s+of|registrar|record\\s+of|statement\\s+of"
                       "|transcript|certified|date\\s+issued|do\\s+not\\s+accept)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    if (titlePattern.match(line).hasMatch()) {
        return true;
    }
    static const QRegularExpression bareTitlePattern(
        QStringLiteral("\\b(?:transcript|registrar|student\\s+record|academic\\s+record)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return bareTitlePattern.match(line).hasMatch();
}

// A plain proper name, for the places people study that are not called a
// university: "Tulsa Technology Center", "Le Cordon Bleu", "Ivy Tech".
//
// Only ever consulted once the document has proved it IS a transcript (see
// the caller) — otherwise the first line of any document at all becomes a
// school, and the first line of a resume is somebody's name.
bool couldBeAnUnlabelledSchoolName(const QString &line)
{
    if (lineIsTheDocumentTalkingAboutItself(line)) {
        return false;
    }
    if (line.length() < shortestUnlabelledSchoolName
            || line.length() > longestUnlabelledSchoolName) {
        return false;
    }
    if (line.contains(QRegularExpression(QStringLiteral("\\d")))) {
        return false;
    }
    if (line.contains(QLatin1Char(':'))) {
        return false;   // "Name: Ultra Jones" is a field, not a letterhead
    }
    // Two or more words. Each word starts with a letter and holds only
    // letters, dots, apostrophes, "&" and hyphens.
    static const QRegularExpression atLeastTwoWordsPattern(
        QStringLiteral("^[A-Za-z][A-Za-z.'&-]*(?:\\s+[A-Za-z][A-Za-z.'&-]*)+$"));
    return atLeastTwoWordsPattern.match(line.trimmed()).hasMatch();
}

} // namespace

// Reads the transcript one line at a time. Blank lines are skipped and not
// counted. For each line:
//   1. Note whether it is a course row. Course rows feed nothing below
//      except the "is this a transcript" check.
//   2. If the line is a course row or holds transcript words, the document
//      behaves like a transcript.
//   3. Near the top, remember the first line that could be a school name
//      without a school word in it. It is used only as a last resort.
//   4. Near the top, the first line with a school word is the school.
//   5. The first award line gives the credential.
//   6. The first labelled subject line gives the field of study.
//   7. Collect term names ("Fall 2000") and bare years for the dates.
//
// After the loop:
//   - No school word found, but the document behaves like a transcript?
//     Use the name remembered in step 3.
//   - Still no school? This is not a transcript. Return nothing.
//   - Dates: the earliest and latest year, written as the document wrote
//     the term when it can be.
ParsedResumeInsights AcademicTranscriptReader::parseTranscriptText(
    const QString &transcriptText) const
{
    ParsedResumeInsights parsedInsights;
    if (transcriptText.trimmed().isEmpty()) {
        return parsedInsights;
    }

    // Same character normalizing the resume parser does. A transcript printed
    // to PDF has the same lookalike spaces and dashes.
    const QStringList allLines =
        withEveryLookalikeCharacterNormalized(transcriptText).split(QLatin1Char('\n'));

    EducationRecord schooling;
    QString letterheadLine;
    QString awardLine;

    QStringList termsFound;
    QStringList yearsFound;

    // Does this document act like a transcript? A resume that got misfiled as
    // one must return nothing, so the caller can send it to the resume parser.
    // The fallback below is too loose to run on anything else.
    bool documentBehavesLikeATranscript = false;
    QString firstPlainNameNearTheTop;

    int lineNumber = 0;
    for (const QString &rawLine : allLines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        ++lineNumber;

        const bool lineIsACourseRow = looksLikeACourseRow(line);
        if (lineIsACourseRow) {
            documentBehavesLikeATranscript = true;
        } else {
            static const QRegularExpression transcriptWordsPattern(
                QStringLiteral("\\b(?:transcript|registrar|grade\\s+point|g\\.?p\\.?a\\.?"
                               "|credit\\s+hours?|semester|quarter|student\\s+(?:id|record|number)"
                               "|degree\\s+(?:awarded|conferred)|coursework)\\b"),
                QRegularExpression::CaseInsensitiveOption);
            if (transcriptWordsPattern.match(line).hasMatch()) {
                documentBehavesLikeATranscript = true;
            }
        }

        if (firstPlainNameNearTheTop.isEmpty()
                && lineNumber <= deepestLineThatCouldBeTheLetterhead
                && !lineIsACourseRow
                && couldBeAnUnlabelledSchoolName(line)) {
            firstPlainNameNearTheTop = line.simplified();
        }

        // --- the school --------------------------------------------------
        if (schooling.schoolName.isEmpty() && !lineIsACourseRow
                && lineNumber <= deepestLineThatCouldBeTheLetterhead
                && InstitutionWords::namesASchool(line)) {
            const QString schoolName = schoolNameOutOf(line);
            if (!schoolName.isEmpty() && schoolName.length() <= longestSchoolName) {
                schooling.schoolName = schoolName;
                letterheadLine = line;
            }
        }

        // --- the credential ----------------------------------------------
        if (schooling.credentialText.isEmpty() && !lineIsACourseRow) {
            const QString credential = credentialOnAwardLine(line);
            if (!credential.isEmpty()) {
                schooling.credentialText = credential;
                awardLine = line;
            }
        }

        // --- the subject -------------------------------------------------
        if (schooling.fieldOfStudyText.isEmpty() && !lineIsACourseRow) {
            const QString subject = subjectOnALabelledLine(line);
            if (!subject.isEmpty()) {
                schooling.fieldOfStudyText = subject;
            }
        }

        // --- the dates ---------------------------------------------------
        //
        // Term headers first, because "Fall 2000" is how the document says it,
        // and dates are shown back in the document's own words. Bare years are
        // the fallback. Course rows are skipped, since a year in a course
        // title is not a term.
        if (!lineIsACourseRow) {
            // A season word, a space, a year. captured(2) is the year.
            static const QRegularExpression termPattern(
                QStringLiteral("\\b(fall|spring|summer|winter|autumn)\\s+(") + yearPattern
                    + QStringLiteral(")\\b"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator terms = termPattern.globalMatch(line);
            while (terms.hasNext()) {
                const QRegularExpressionMatch term = terms.next();
                termsFound.append(term.captured(0).simplified());
                yearsFound.append(term.captured(2));
            }

            // Every year on the line, including the ones already read as part of
            // a term. Those repeats do no harm: only the earliest and latest
            // are kept.
            static const QRegularExpression loneYearPattern(yearPattern);
            QRegularExpressionMatchIterator years = loneYearPattern.globalMatch(line);
            while (years.hasNext()) {
                yearsFound.append(years.next().captured(0));
            }
        }
    }

    if (schooling.schoolName.isEmpty() && documentBehavesLikeATranscript
            && !firstPlainNameNearTheTop.isEmpty()) {
        // Trade schools and technology centers have names with none of the
        // usual school words in them. Their students need this to work too.
        schooling.schoolName = firstPlainNameNearTheTop;
        letterheadLine = firstPlainNameNearTheTop;
    }

    if (schooling.schoolName.isEmpty()) {
        // Not a transcript, or not one this reader handles. Return nothing so
        // the caller falls back to the resume parser.
        return parsedInsights;
    }

    // --- first term to last ---------------------------------------------
    //
    // If the document names its terms, use only those years. Transcript
    // headers carry an issue date like "Issued: 14 March 2019", and counting
    // bare years would read that as the year the student finished.
    if (!termsFound.isEmpty()) {
        QStringList yearsFromTermsOnly;
        for (const QString &term : termsFound) {
            yearsFromTermsOnly.append(term.right(4));
        }
        yearsFound = yearsFromTermsOnly;
    }

    if (!yearsFound.isEmpty()) {
        // Years are four digits, so comparing them as text gives the same
        // order as comparing them as numbers.
        QString earliestYear = yearsFound.first();
        QString latestYear = yearsFound.first();
        for (const QString &year : yearsFound) {
            if (year < earliestYear) earliestYear = year;
            if (year > latestYear)   latestYear = year;
        }

        // Use the document's own wording: "Fall 2000", not "2000".
        //
        // Looks through the terms for ones that end in this year. With one,
        // that term is used. With several, the earliest or latest season in
        // seasonOrder wins. With none, the bare year is used.
        auto termWorded = [&termsFound](const QString &year, bool wantEarliest) -> QString {
            QString chosenTerm;
            for (const QString &term : termsFound) {
                if (!term.endsWith(year)) {
                    continue;
                }
                if (chosenTerm.isEmpty()) {
                    chosenTerm = term;
                    continue;
                }
                // Within one year, sort seasons in school-year order so the
                // first and last term are correct.
                static const QStringList seasonOrder = {
                    QStringLiteral("spring"), QStringLiteral("summer"),
                    QStringLiteral("fall"), QStringLiteral("autumn"),
                    QStringLiteral("winter")
                };
                const int thisPlace = seasonOrder.indexOf(term.section(' ', 0, 0).toLower());
                const int chosenPlace =
                    seasonOrder.indexOf(chosenTerm.section(' ', 0, 0).toLower());
                if (wantEarliest ? (thisPlace < chosenPlace) : (thisPlace > chosenPlace)) {
                    chosenTerm = term;
                }
            }
            return chosenTerm.isEmpty() ? year : chosenTerm;
        };

        schooling.startDateText = termWorded(earliestYear, true);
        schooling.endDateText = termWorded(latestYear, false);

        // Only one term. That is an end date, not a range.
        if (schooling.startDateText == schooling.endDateText) {
            schooling.startDateText.clear();
        }
    }

    schooling.sourceLineText = awardLine.isEmpty()
        ? letterheadLine
        : letterheadLine + QStringLiteral("  /  ") + awardLine;

    parsedInsights.educationRecords.append(schooling);
    return parsedInsights;
}
