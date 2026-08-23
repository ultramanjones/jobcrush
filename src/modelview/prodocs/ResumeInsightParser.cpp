#include "ResumeInsightParser.h"

#include <QRegularExpression>

#include "PlainTextNormalizer.h"

namespace {

// --- Section headings ----------------------------------------------------
//
// A resume is a set of labelled blocks, and finding those labels is most of
// the job. A heading is a SHORT line whose words match one of these — length
// matters, because "my experience includes twelve years of…" is a sentence,
// not a heading.
constexpr int longestPlausibleHeadingLength = 60;

enum class SectionKind { Unknown, Experience, Education, Skills, OtherKnown };

QString headingKeyOf(const QString &line)
{
    static const QRegularExpression nonLetterPattern(QStringLiteral("[^a-z ]+"));
    QString headingKey = line.toLower();
    headingKey.replace(nonLetterPattern, QStringLiteral(" "));
    return headingKey.simplified();
}

SectionKind sectionKindForHeading(const QString &line)
{
    if (line.trimmed().length() > longestPlausibleHeadingLength) {
        return SectionKind::Unknown;
    }
    const QString headingKey = headingKeyOf(line);
    if (headingKey.isEmpty()) {
        return SectionKind::Unknown;
    }

    static const QStringList experienceHeadings = {
        QStringLiteral("experience"), QStringLiteral("work experience"),
        QStringLiteral("professional experience"), QStringLiteral("employment"),
        QStringLiteral("employment history"), QStringLiteral("work history"),
        QStringLiteral("career history"), QStringLiteral("relevant experience"),
        QStringLiteral("professional background"),
    };
    static const QStringList educationHeadings = {
        QStringLiteral("education"), QStringLiteral("academic background"),
        QStringLiteral("education and training"), QStringLiteral("academics"),
        QStringLiteral("educational background"),
    };
    static const QStringList skillsHeadings = {
        QStringLiteral("skills"), QStringLiteral("technical skills"),
        QStringLiteral("core competencies"), QStringLiteral("technologies"),
        QStringLiteral("competencies"), QStringLiteral("areas of expertise"),
    };
    // Recognised so they can END a section, even though nothing is read
    // from them. Without this, a Projects block would be swallowed into
    // Experience and become imaginary jobs.
    static const QStringList otherKnownHeadings = {
        QStringLiteral("projects"), QStringLiteral("certifications"),
        QStringLiteral("references"), QStringLiteral("summary"),
        QStringLiteral("objective"), QStringLiteral("awards"),
        QStringLiteral("publications"), QStringLiteral("interests"),
        QStringLiteral("volunteer"), QStringLiteral("languages"),
        QStringLiteral("professional summary"), QStringLiteral("contact"),
        QStringLiteral("honors"), QStringLiteral("activities"),
    };

    if (experienceHeadings.contains(headingKey)) return SectionKind::Experience;
    if (educationHeadings.contains(headingKey))  return SectionKind::Education;
    if (skillsHeadings.contains(headingKey))     return SectionKind::Skills;
    if (otherKnownHeadings.contains(headingKey)) return SectionKind::OtherKnown;
    return SectionKind::Unknown;
}

// --- Dates ---------------------------------------------------------------
//
// A date range is the most reliable anchor in a resume: almost every job and
// degree carries one, and almost nothing else does. It is what marks where
// one entry stops and the next begins.
const QString monthWordPattern =
    QStringLiteral("(?:jan|feb|mar|apr|may|jun|jul|aug|sep|oct|nov|dec)[a-z]*\\.?");
const QString yearPattern = QStringLiteral("(?:19|20)\\d{2}");
const QString rangeEndpointPattern =
    QStringLiteral("(?:") + monthWordPattern + QStringLiteral("\\s+)?") + yearPattern;
const QString openEndedPattern =
    QStringLiteral("(?:present|current|now|ongoing|to date)");

const QRegularExpression &dateRangePattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("(") + rangeEndpointPattern + QStringLiteral(")")
        + QStringLiteral("\\s*(?:-|–|—|to|until|through|thru)\\s*")
        + QStringLiteral("(") + rangeEndpointPattern + QStringLiteral("|")
        + openEndedPattern + QStringLiteral(")"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// --- Telling a job title from an employer --------------------------------
//
// Both sit on the same line, in either order, separated by anything. Rather
// than guess by position — which is wrong about half the time — look for what
// each piece actually IS.
bool looksLikeRoleTitle(const QString &piece)
{
    static const QStringList roleWords = {
        QStringLiteral("engineer"), QStringLiteral("developer"), QStringLiteral("programmer"),
        QStringLiteral("manager"), QStringLiteral("designer"), QStringLiteral("analyst"),
        QStringLiteral("director"), QStringLiteral("specialist"), QStringLiteral("consultant"),
        QStringLiteral("technician"), QStringLiteral("architect"), QStringLiteral("administrator"),
        QStringLiteral("intern"), QStringLiteral("lead"), QStringLiteral("scientist"),
        QStringLiteral("officer"), QStringLiteral("coordinator"), QStringLiteral("assistant"),
        QStringLiteral("supervisor"), QStringLiteral("writer"), QStringLiteral("teacher"),
        QStringLiteral("nurse"), QStringLiteral("chef"), QStringLiteral("server"),
        QStringLiteral("driver"), QStringLiteral("clerk"), QStringLiteral("associate"),
        QStringLiteral("operator"), QStringLiteral("foreman"), QStringLiteral("apprentice"),
        // Trades, service and office work. Leaving these out would mean the
        // app read a software resume well and everybody else's badly, which
        // is not the job it was built for.
        QStringLiteral("cook"), QStringLiteral("baker"), QStringLiteral("cashier"),
        QStringLiteral("bartender"), QStringLiteral("barista"), QStringLiteral("host"),
        QStringLiteral("attendant"), QStringLiteral("guard"), QStringLiteral("mechanic"),
        QStringLiteral("welder"), QStringLiteral("carpenter"), QStringLiteral("electrician"),
        QStringLiteral("plumber"), QStringLiteral("painter"), QStringLiteral("laborer"),
        QStringLiteral("custodian"), QStringLiteral("janitor"), QStringLiteral("receptionist"),
        QStringLiteral("secretary"), QStringLiteral("bookkeeper"), QStringLiteral("accountant"),
        QStringLiteral("paralegal"), QStringLiteral("therapist"), QStringLiteral("aide"),
        QStringLiteral("caregiver"), QStringLiteral("stocker"), QStringLiteral("packer"),
        QStringLiteral("installer"), QStringLiteral("inspector"), QStringLiteral("dispatcher"),
        QStringLiteral("estimator"), QStringLiteral("machinist"), QStringLiteral("assembler"),
        QStringLiteral("courier"), QStringLiteral("stylist"), QStringLiteral("groundskeeper"),
        QStringLiteral("waiter"), QStringLiteral("waitress"), QStringLiteral("busser"),
        QStringLiteral("dishwasher"), QStringLiteral("trainer"), QStringLiteral("tutor"),
        QStringLiteral("librarian"), QStringLiteral("pharmacist"), QStringLiteral("dj"),
    };
    const QString loweredPiece = piece.toLower();
    for (const QString &roleWord : roleWords) {
        if (loweredPiece.contains(roleWord)) {
            return true;
        }
    }
    return false;
}

bool looksLikeEmployerName(const QString &piece)
{
    static const QStringList employerWords = {
        QStringLiteral("inc"), QStringLiteral("llc"), QStringLiteral("ltd"),
        QStringLiteral("corp"), QStringLiteral("corporation"), QStringLiteral("company"),
        QStringLiteral("technologies"), QStringLiteral("technology"), QStringLiteral("systems"),
        QStringLiteral("solutions"), QStringLiteral("group"), QStringLiteral("labs"),
        QStringLiteral("laboratories"), QStringLiteral("studio"), QStringLiteral("software"),
        QStringLiteral("industries"), QStringLiteral("hospital"), QStringLiteral("bank"),
        QStringLiteral("university"), QStringLiteral("college"), QStringLiteral("associates"),
        QStringLiteral("partners"), QStringLiteral("services"), QStringLiteral("holdings"),
    };
    const QString loweredPiece = piece.toLower();
    for (const QString &employerWord : employerWords) {
        if (loweredPiece.contains(employerWord)) {
            return true;
        }
    }
    return false;
}

// Some resumes — especially ones converted from markdown or plain text —
// squash the employer and the job title into one run of words with nothing
// between them: "Longwave Corporation Senior Software Engineer". Read whole,
// that lands in the job-title box with the employer left blank.
//
// When one piece plainly contains both, cut it where the job title starts.
bool splitCombinedEmployerAndRole(const QString &piece,
                                  QString &employerOut,
                                  QString &roleOut)
{
    if (!looksLikeEmployerName(piece) || !looksLikeRoleTitle(piece)) {
        return false;
    }

    static const QRegularExpression roleStartPattern(
        QStringLiteral("\\b(?:senior|junior|lead|principal|staff|chief|head)?\\s*"
                       "(?:software|systems?|web|front[- ]?end|back[- ]?end|full[- ]?stack"
                       "|qa|data|network|electrical|mechanical|civil)?\\s*"
                       "(?:engineer|developer|programmer|manager|designer|analyst|director"
                       "|architect|administrator|technician|consultant|specialist)\\b"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch roleMatch = roleStartPattern.match(piece);
    if (!roleMatch.hasMatch() || roleMatch.capturedStart() <= 0) {
        return false; // nothing before the title, so there is nothing to split off
    }

    employerOut = piece.left(roleMatch.capturedStart()).trimmed();
    roleOut = piece.mid(roleMatch.capturedStart()).trimmed();
    return !employerOut.isEmpty() && !roleOut.isEmpty();
}

// Splits the non-date part of an entry line into its pieces.
// Taking a date out of "Acme Technologies Inc. (Jan 2020 - Present)" leaves
// "Acme Technologies Inc. ()". Sweeping up that debris keeps stray brackets
// out of the employer box, where they look like a bug because they are one.
QString withoutPunctuationDebris(const QString &text)
{
    static const QRegularExpression emptyBracketsPattern(
        QStringLiteral("\\(\\s*\\)|\\[\\s*\\]|\\{\\s*\\}"));
    static const QRegularExpression danglingEdgePattern(
        QStringLiteral("^[\\s,;:|\\-–—•·(\\[]+|[\\s,;:|\\-–—•·)\\]]+$"));

    QString cleanedText = text;
    cleanedText.remove(emptyBracketsPattern);
    cleanedText.remove(danglingEdgePattern);

    // A bracket that has a partner is not debris. "Verb Surgical (Johnson &
    // Johnson)" lost its closing bracket to the sweep above and came out
    // looking like the parser had given up halfway through the name.
    const int openBracketCount = text.count(QLatin1Char('('));
    const int closeBracketCount = text.count(QLatin1Char(')'));
    if (openBracketCount == closeBracketCount && openBracketCount > 0
            && cleanedText.count(QLatin1Char('(')) != cleanedText.count(QLatin1Char(')'))) {
        cleanedText = text;
        cleanedText.remove(emptyBracketsPattern);
        static const QRegularExpression danglingEdgeKeepingBracketsPattern(
            QStringLiteral("^[\\s,;:|\\-–—•·]+|[\\s,;:|\\-–—•·]+$"));
        cleanedText.remove(danglingEdgeKeepingBracketsPattern);
    }

    return cleanedText.simplified();
}

QStringList piecesOfEntryLine(const QString &lineRemainder)
{
    // A RUN OF SPACES is a separator too. Resumes lay their education and
    // employment out in columns, and once the PDF is flattened the only thing
    // left of the column gap is the whitespace. Without this, "2000 – 2001
    // <gap> Auburn University" reads as one piece and the school ends up
    // called "2001 Auburn University". Three is the threshold: two spaces
    // after a full stop is typing, three is layout.
    static const QRegularExpression separatorPattern(
        QStringLiteral("\\s+[–—|·•]\\s+|\\s+-\\s+|\\s+at\\s+|,\\s+|\\t+|\\s{3,}"),
        QRegularExpression::CaseInsensitiveOption);

    QStringList pieces;
    for (const QString &rawPiece : lineRemainder.split(separatorPattern, Qt::SkipEmptyParts)) {
        const QString piece = withoutPunctuationDebris(rawPiece);
        if (!piece.isEmpty()) {
            pieces.append(piece);
        }
    }
    return pieces;
}

// True when a piece reads like prose rather than a name — a long stretch of
// words, or something that ends in a full stop. Those belong in the summary,
// not in the employer box.
bool looksLikeProse(const QString &piece)
{
    if (piece.length() > 60) {
        return true;
    }
    if (!piece.endsWith(QLatin1Char('.'))) {
        return false;
    }

    // A full stop is not proof of a sentence. "Acme Robotics, Inc." ends in
    // one, and calling that prose threw away the employer on every job at a
    // company whose name ends in Inc., Ltd. or Corp. — and then, because the
    // name was still sitting there unclaimed, read it as part of the PREVIOUS
    // job's description. Two wrong entries out of one punctuation mark.
    //
    // A closing abbreviation is short. A sentence is not.
    const QStringList wordsOfThePiece =
        piece.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (wordsOfThePiece.count() > 4) {
        return true;
    }
    QString lastWord = wordsOfThePiece.isEmpty() ? QString() : wordsOfThePiece.last();
    lastWord.remove(QRegularExpression(QStringLiteral("[^A-Za-z]")));
    return lastWord.length() > 4;
}

// --- Education -----------------------------------------------------------
QString credentialPhraseIn(const QString &line)
{
    // The alternatives are spelled out rather than using a loose "of
    // <anything>" wildcard. A wildcard swallowed the subject too — "Master of
    // Science in Software Engineering" came back as the DEGREE, leaving the
    // subject box empty. Longest alternatives come first so the regex prefers
    // "Master of Science" over a bare "Master".
    static const QRegularExpression credentialPattern(
        QStringLiteral("\\b(?:"
                       "(?:bachelor|master|associate|doctor)(?:'s)?\\s+of\\s+"
                       "(?:applied\\s+science|business\\s+administration|fine\\s+arts"
                       "|science|arts|engineering|education|nursing|philosophy)"
                       "|ph\\.?\\s?d\\.?|doctorate"
                       "|bachelor(?:'s)?|master(?:'s)?|associate(?:'s)?"
                       "|m\\.?b\\.?a\\.?|b\\.?s\\.?c?\\.?|b\\.?a\\.?"
                       "|m\\.?s\\.?|m\\.?a\\.?|a\\.?a\\.?s?\\.?"
                       "|certificate|certification|diploma|ged"
                       ")\\b"),
        QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = credentialPattern.match(line);
    if (!match.hasMatch()) {
        return QString();
    }

    // A word boundary can't sit after a full stop, so "B.S." matches as "B.S"
    // and every degree row loses its last character. Put it back.
    int matchEnd = match.capturedEnd(0);
    if (matchEnd < line.length() && line.at(matchEnd) == QLatin1Char('.')) {
        ++matchEnd;
    }
    return line.mid(match.capturedStart(0), matchEnd - match.capturedStart(0)).trimmed();
}

// The words that mark an institution. One list, so the detector below and
// the splitter below THAT can never disagree about what a school looks like.
const QStringList &wordsThatNameASchool()
{
    static const QStringList schoolWords = {
        QStringLiteral("university"), QStringLiteral("college"), QStringLiteral("institute"),
        QStringLiteral("academy"), QStringLiteral("school"), QStringLiteral("polytechnic"),
        QStringLiteral("seminary"), QStringLiteral("conservatory"),
        // "Univ Alabama Huntsville" is how it appears on a real transcript;
        // matching only the full word would have missed it entirely.
        QStringLiteral("univ"),
    };
    return schoolWords;
}

bool looksLikeSchoolName(const QString &piece)
{
    const QString loweredPiece = piece.toLower();
    for (const QString &schoolWord : wordsThatNameASchool()) {
        if (loweredPiece.contains(schoolWord)) {
            return true;
        }
    }
    return false;
}

// One word, stripped of the punctuation transcripts sprinkle around, asked
// whether it is the word that names an institution.
bool thisWordNamesASchool(const QString &word)
{
    QString bareWord = word.toLower();
    static const QRegularExpression edgePunctuationPattern(
        QStringLiteral("^[^a-z]+|[^a-z]+$"));
    bareWord.remove(edgePunctuationPattern);
    if (bareWord.isEmpty()) {
        return false;
    }
    for (const QString &schoolWord : wordsThatNameASchool()) {
        if (bareWord == schoolWord || bareWord == schoolWord + QStringLiteral("s")) {
            return true;
        }
    }
    return false;
}

// Small words that belong INSIDE an institution's name rather than ending it.
bool wordBelongsInsideASchoolName(const QString &word)
{
    static const QStringList joiningWords = {
        QStringLiteral("of"), QStringLiteral("at"), QStringLiteral("the"),
        QStringLiteral("and"), QStringLiteral("for"), QStringLiteral("in"),
    };
    return joiningWords.contains(word.toLower());
}

bool wordStartsAProperNoun(const QString &word)
{
    return !word.isEmpty() && word.at(0).isUpper();
}

// A piece that is nothing but a year: "2001". Transcripts and resumes list
// these right after the school they belong to.
bool pieceIsABareYear(const QString &piece)
{
    static const QRegularExpression bareYearPattern(QStringLiteral("^(19|20)\\d{2}$"));
    return bareYearPattern.match(piece.trimmed()).hasMatch();
}

// Pulls apart a run of text that names MORE THAN ONE institution.
//
// Transcripts do this constantly: a degree page lists the school that granted
// it and then, with nothing between them, every school that sent transfer
// credit. Extracted from a PDF it arrives as one line with no separator, and
// a school box that reads "Pennsylvania State University Community College of
// Allegheny County Univ Alabama Huntsville" is not one school with a strange
// name — it is three schools the user now has to untangle by hand.
//
// The shape of an institution's name decides where it ends:
//   - a name that BEGINS with the marker word keeps going ("Univ Alabama
//     Huntsville", "University of Alabama in Huntsville"),
//   - a name that ENDS with it stops there ("Pennsylvania State University"),
//   - a connector after the marker pulls the rest in ("College of Allegheny
//     County").
//
// One institution in, one out — the common case costs nothing.
QStringList institutionNamesIn(const QString &text)
{
    // UseUnicodeProperties because "\s" alone walks straight past a
    // non-breaking space. DocumentTextExtractor normalizes those away before
    // the parser ever sees them; this is the second lock on the same door,
    // for text that arrives from somewhere else one day.
    static const QRegularExpression anyWhitespacePattern(
        QStringLiteral("\\s+"), QRegularExpression::UseUnicodePropertiesOption);
    const QStringList words = text.split(anyWhitespacePattern, Qt::SkipEmptyParts);

    QList<int> markerWordIndexes;
    for (int wordIndex = 0; wordIndex < words.count(); ++wordIndex) {
        if (thisWordNamesASchool(words.at(wordIndex))) {
            markerWordIndexes.append(wordIndex);
        }
    }
    if (markerWordIndexes.count() < 2) {
        return QStringList{text.trimmed()};
    }

    QStringList institutionNames;
    int spanStartIndex = 0;

    for (int markerNumber = 0; markerNumber < markerWordIndexes.count(); ++markerNumber) {
        const int markerIndex = markerWordIndexes.at(markerNumber);
        const int nextMarkerIndex = (markerNumber + 1 < markerWordIndexes.count())
            ? markerWordIndexes.at(markerNumber + 1)
            : words.count();

        int spanEndIndex = markerIndex;

        // A leading marker ("Univ …", "University of …") owns what follows it.
        // A trailing one ("… State University") does not.
        const bool markerLeadsTheName = (markerIndex == spanStartIndex)
            || (markerIndex + 1 < words.count()
                && wordBelongsInsideASchoolName(words.at(markerIndex + 1)));

        if (markerLeadsTheName) {
            int candidateIndex = markerIndex + 1;
            while (candidateIndex < nextMarkerIndex
                   && (wordStartsAProperNoun(words.at(candidateIndex))
                       || wordBelongsInsideASchoolName(words.at(candidateIndex)))) {
                spanEndIndex = candidateIndex;
                ++candidateIndex;
            }
            // A connector cannot be the last word of a name — "College of"
            // is not a school.
            while (spanEndIndex > markerIndex
                   && wordBelongsInsideASchoolName(words.at(spanEndIndex))) {
                --spanEndIndex;
            }
        }

        if (spanEndIndex < spanStartIndex) {
            spanEndIndex = spanStartIndex;
        }

        QStringList wordsOfThisName;
        for (int wordIndex = spanStartIndex; wordIndex <= spanEndIndex; ++wordIndex) {
            wordsOfThisName.append(words.at(wordIndex));
        }
        const QString institutionName = wordsOfThisName.join(QLatin1Char(' ')).trimmed();
        if (!institutionName.isEmpty()) {
            institutionNames.append(institutionName);
        }

        spanStartIndex = spanEndIndex + 1;
    }

    // Anything after the last institution is a date, a degree or a grade —
    // it is not a school, and inventing a school out of it would be worse
    // than leaving the run alone.

    // If the split produced anything that no longer reads as a school, the
    // guess was wrong. Hand back the original rather than a mess.
    for (const QString &institutionName : institutionNames) {
        if (!looksLikeSchoolName(institutionName)) {
            return QStringList{text.trimmed()};
        }
    }
    return institutionNames.isEmpty() ? QStringList{text.trimmed()} : institutionNames;
}

// The subject, when the line spells it out: "Bachelor of Science IN Computer
// Science", "B.S., Computer Science". Never the school — see below.
QString fieldOfStudyIn(const QString &line, const QString &credentialPhrase)
{
    if (credentialPhrase.isEmpty()) {
        return QString();
    }
    const int credentialEnd = line.indexOf(credentialPhrase, 0, Qt::CaseInsensitive)
                              + credentialPhrase.length();
    QString tail = line.mid(credentialEnd).trimmed();

    // "in" and "of" need their word boundary. Without one this matched the
    // "In" inside "Information Sciences" and handed the user a subject called
    // "formation Sciences" — a typo invented by the app, sitting in a box on
    // the one screen whose whole job is being trustworthy about their past.
    static const QRegularExpression leadInPattern(
        QStringLiteral("^(?:(?:in|of)\\b|[,:]|-|–|—)\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch leadIn = leadInPattern.match(tail);
    if (leadIn.hasMatch()) {
        tail = tail.mid(leadIn.capturedLength()).trimmed();
    }
    // No lead-in word at all is fine: "B.S. Information Sciences" writes the
    // subject straight after the degree. Whatever follows still has to pass
    // every test below before it goes anywhere near the subject box.
    if (tail.isEmpty()) {
        return QString();
    }

    // Stop at the next separator — anything past it is the school or a date.
    // The middle dot belongs in here. DocumentTextExtractor now turns every
    // bullet a designer might have used into "·", so it is THE separator a
    // resume uses between fields — and without it the subject box swallowed
    // "Computer Simulation / Game Development · 2012 · GPA 3.75" whole.
    static const QRegularExpression tailStopPattern(
        QStringLiteral("\\s*[,|–—•·]|\\s+-\\s+|\\s{3,}"));
    const QRegularExpressionMatch stop = tailStopPattern.match(tail);
    if (stop.hasMatch()) {
        tail = tail.left(stop.capturedStart()).trimmed();
    }
    // "Certificate, Univ Alabama Huntsville, 2011" — the words after the
    // credential are the SCHOOL, not the subject. Handing a school name to
    // the subject box would be a confident, visible error on every row.
    if (looksLikeProse(tail) || looksLikeSchoolName(tail)) {
        return QString();
    }
    // "GED, 1998" — what follows the comma is the year, not the subject. A
    // year in the subject box is the app telling somebody they studied 1998.
    static const QRegularExpression nothingButNumbersPattern(
        QStringLiteral("^[^A-Za-z]*$"));
    if (nothingButNumbersPattern.match(tail).hasMatch()) {
        return QString();
    }
    return tail;
}

} // namespace

ParsedResumeInsights ResumeInsightParser::parseResumeText(const QString &rawResumeText) const
{
    // Normalized HERE as well as at extraction, and that is not belt and
    // braces — it is the only thing that helps a document imported by an
    // older build. Its text is already sitting in the database full of
    // non-breaking spaces, and re-reading it would reproduce the same mangled
    // entries forever if the parser trusted what it was handed.
    const QString resumeText = withEveryLookalikeCharacterNormalized(rawResumeText);

    ParsedResumeInsights parsedInsights;
    if (resumeText.trimmed().isEmpty()) {
        return parsedInsights;
    }

    const QStringList allLines = resumeText.split(QLatin1Char('\n'));

    SectionKind currentSection = SectionKind::Unknown;
    WorkExperience workEntryBeingBuilt;
    bool buildingWorkEntry = false;
    QStringList recentNonEmptyLines;   // for entries whose name sits on the line above

    auto finishWorkEntry = [&]() {
        if (!buildingWorkEntry) {
            return;
        }
        // An entry with nothing but dates says nothing. Dropping it is better
        // than showing the user a blank row and calling it a job.
        if (!workEntryBeingBuilt.employerName.isEmpty()
                || !workEntryBeingBuilt.roleTitle.isEmpty()) {
            parsedInsights.workExperiences.append(workEntryBeingBuilt);
        }
        workEntryBeingBuilt = WorkExperience();
        buildingWorkEntry = false;
    };

    for (const QString &rawLine : allLines) {
        const QString line = rawLine.trimmed();

        if (line.isEmpty()) {
            continue;
        }

        const SectionKind headingKind = sectionKindForHeading(line);
        if (headingKind != SectionKind::Unknown) {
            finishWorkEntry();
            currentSection = headingKind;
            recentNonEmptyLines.clear();
            continue;
        }

        if (currentSection == SectionKind::Skills) {
            static const QRegularExpression skillSeparatorPattern(
                QStringLiteral("[,;|•·/]|\\s{3,}"));
            for (const QString &rawSkill : line.split(skillSeparatorPattern, Qt::SkipEmptyParts)) {
                const QString skillTerm = rawSkill.trimmed();
                // One or two characters is punctuation noise; a long stretch
                // is a sentence about skills, not a skill.
                if (skillTerm.length() >= 2 && skillTerm.length() <= 40
                        && !parsedInsights.skillTerms.contains(skillTerm, Qt::CaseInsensitive)) {
                    parsedInsights.skillTerms.append(skillTerm);
                }
            }
            continue;
        }

        const QRegularExpressionMatch dateMatch = dateRangePattern().match(line);

        if (currentSection == SectionKind::Experience) {
            if (dateMatch.hasMatch()) {
                // A date range starts a new job. Close the previous one first.
                finishWorkEntry();
                buildingWorkEntry = true;
                workEntryBeingBuilt.startDateText = dateMatch.captured(1).trimmed();
                workEntryBeingBuilt.endDateText = dateMatch.captured(2).trimmed();
                workEntryBeingBuilt.sourceLineText = line;

                QString lineRemainder = line;
                lineRemainder.remove(dateMatch.capturedStart(), dateMatch.capturedLength());

                QStringList pieces = piecesOfEntryLine(lineRemainder);
                // Nothing else on the line? The name is almost always the
                // line directly above it.
                if (pieces.isEmpty() && !recentNonEmptyLines.isEmpty()) {
                    pieces = piecesOfEntryLine(recentNonEmptyLines.last());
                    workEntryBeingBuilt.sourceLineText =
                        recentNonEmptyLines.last() + QStringLiteral("  /  ") + line;
                }

                // Did the WRITER already separate the fields? A line reading
                // "Contract UI Software Engineer | Specific Impulses Inc." has
                // said, in as many words, where the title ends and the
                // employer begins. Guessing a second cut inside one of those
                // pieces then overrules the person who wrote the resume — and
                // it did: the title became "Software Engineer" and the
                // employer became "Contract UI", which is not a company.
                const bool writerAlreadySeparatedTheFields =
                    lineRemainder.contains(QLatin1Char('|'))
                    || lineRemainder.contains(QChar(0x00B7));

                for (const QString &piece : pieces) {
                    QString splitEmployer;
                    QString splitRole;
                    if (!writerAlreadySeparatedTheFields
                            && workEntryBeingBuilt.employerName.isEmpty()
                            && workEntryBeingBuilt.roleTitle.isEmpty()
                            && splitCombinedEmployerAndRole(piece, splitEmployer, splitRole)) {
                        workEntryBeingBuilt.employerName = splitEmployer;
                        workEntryBeingBuilt.roleTitle = splitRole;
                        continue;
                    }

                    if (looksLikeRoleTitle(piece) && workEntryBeingBuilt.roleTitle.isEmpty()) {
                        workEntryBeingBuilt.roleTitle = piece;
                    } else if (looksLikeEmployerName(piece)
                               && workEntryBeingBuilt.employerName.isEmpty()) {
                        workEntryBeingBuilt.employerName = piece;
                    } else if (!looksLikeProse(piece)
                               && workEntryBeingBuilt.employerName.isEmpty()) {
                        // Unrecognised but short — most likely the employer,
                        // and one click fixes it if not.
                        workEntryBeingBuilt.employerName = piece;
                    } else {
                        if (!workEntryBeingBuilt.summaryText.isEmpty()) {
                            workEntryBeingBuilt.summaryText += QStringLiteral("  ");
                        }
                        workEntryBeingBuilt.summaryText += piece;
                    }
                }

                // The commonest resume layout in the world puts the employer
                // on its own line and the job title with the dates underneath:
                //
                //     Longwave Corporation
                //     Senior Software Engineer            2016 - 2019
                //
                // Reading only the dated line finds the title and leaves the
                // employer blank on every single job — so when the employer is
                // still missing, look up one line.
                if (workEntryBeingBuilt.employerName.isEmpty()
                        && !recentNonEmptyLines.isEmpty()) {
                    const QString lineAbove = recentNonEmptyLines.last().trimmed();
                    const bool lineAboveIsUsable =
                        !looksLikeProse(lineAbove)
                        && !looksLikeRoleTitle(lineAbove)
                        && !dateRangePattern().match(lineAbove).hasMatch()
                        && sectionKindForHeading(lineAbove) == SectionKind::Unknown;
                    if (lineAboveIsUsable) {
                        workEntryBeingBuilt.employerName = withoutPunctuationDebris(lineAbove);
                        workEntryBeingBuilt.sourceLineText =
                            lineAbove + QStringLiteral("  /  ") + line;

                        // That line was read a moment ago as one more sentence
                        // about the PREVIOUS job, because nothing had yet said
                        // it was a heading for this one. Now that it turns out
                        // to be an employer, take it back out of the job above
                        // — otherwise every description on the page ends with
                        // the name of the next company down.
                        if (!parsedInsights.workExperiences.isEmpty()) {
                            WorkExperience &previousEntry = parsedInsights.workExperiences.last();
                            const QString trailingText = lineAbove.trimmed();
                            if (previousEntry.summaryText.endsWith(trailingText)) {
                                previousEntry.summaryText.chop(trailingText.length());
                                previousEntry.summaryText = previousEntry.summaryText.trimmed();
                            }
                        }
                    }
                }
            } else if (buildingWorkEntry) {
                // Bullets and description lines under the current job.
                QString descriptionLine = line;
                static const QRegularExpression bulletPattern(
                    QStringLiteral("^[-•·*▪o]\\s+"));
                descriptionLine.remove(bulletPattern);

                if (!workEntryBeingBuilt.summaryText.isEmpty()) {
                    workEntryBeingBuilt.summaryText += QStringLiteral("  ");
                }
                workEntryBeingBuilt.summaryText += descriptionLine.trimmed();
            }
        } else if (currentSection == SectionKind::Education) {
            const QString credentialPhrase = credentialPhraseIn(line);
            const bool lineNamesASchool = looksLikeSchoolName(line);

            if (credentialPhrase.isEmpty() && !lineNamesASchool) {
                // Neither a credential nor a school. If it carries dates it is
                // almost certainly the "2000 - 2001" line under the school
                // just above, so give those dates to that entry rather than
                // leaving it looking undated.
                if (dateMatch.hasMatch() && !parsedInsights.educationRecords.isEmpty()
                        && parsedInsights.educationRecords.last().endDateText.isEmpty()) {
                    EducationRecord &previousRecord = parsedInsights.educationRecords.last();
                    previousRecord.startDateText = dateMatch.captured(1).trimmed();
                    previousRecord.endDateText = dateMatch.captured(2).trimmed();
                    previousRecord.sourceLineText += QStringLiteral("  /  ") + line;
                }
                recentNonEmptyLines.append(line);
                continue;
            }

            EducationRecord educationRecord;
            educationRecord.sourceLineText = line;
            educationRecord.credentialText = credentialPhrase;
            educationRecord.fieldOfStudyText = fieldOfStudyIn(line, credentialPhrase);

            // Every school this line names, in the order it names them, each
            // paired with where it sat so the years beside it can be found.
            //
            // "Tulsa Community College · 2000 – 2001    Auburn University ·
            // 1993 – 1994" is ONE line naming TWO schools, and a resume that
            // sets its education out in two columns produces exactly this.
            // Taking only the first school silently loses the second, which
            // is worse than mangling it: the user has no way to notice.
            struct SchoolFoundOnThisLine {
                QString schoolName;
                int pieceIndex = 0;   // where it sat, for finding its years
            };
            QList<SchoolFoundOnThisLine> schoolsFoundOnThisLine;

            const QStringList piecesOfThisLine = piecesOfEntryLine(line);
            for (int pieceIndex = 0; pieceIndex < piecesOfThisLine.count(); ++pieceIndex) {
                QString schoolNamePiece = piecesOfThisLine.at(pieceIndex);
                if (!looksLikeSchoolName(schoolNamePiece)) {
                    continue;
                }

                // "Auburn University B.S. in Computer Science" is a school
                // AND a degree with nothing between them. Cut at the
                // degree, or the school box ends up holding the lot.
                if (!credentialPhrase.isEmpty()) {
                    const int credentialStart =
                        schoolNamePiece.indexOf(credentialPhrase, 0, Qt::CaseInsensitive);
                    if (credentialStart > 0) {
                        schoolNamePiece = schoolNamePiece.left(credentialStart);
                    }
                }

                // A single piece can still hold several institutions when a
                // transcript runs transfer credit together with no separator
                // at all. institutionNamesIn takes those apart.
                for (const QString &institutionName :
                         institutionNamesIn(withoutPunctuationDebris(schoolNamePiece))) {
                    if (!institutionName.isEmpty()) {
                        schoolsFoundOnThisLine.append({ institutionName, pieceIndex });
                    }
                }
            }

            // Schools beyond the first. Each becomes its own row.
            QList<SchoolFoundOnThisLine> additionalSchoolsFound;
            if (!schoolsFoundOnThisLine.isEmpty()) {
                educationRecord.schoolName = schoolsFoundOnThisLine.first().schoolName;
                for (int extraIndex = 1; extraIndex < schoolsFoundOnThisLine.count();
                     ++extraIndex) {
                    additionalSchoolsFound.append(schoolsFoundOnThisLine.at(extraIndex));
                }
            }
            // The credential and the school often sit on consecutive lines.
            if (educationRecord.schoolName.isEmpty() && !recentNonEmptyLines.isEmpty()
                    && looksLikeSchoolName(recentNonEmptyLines.last())) {
                // Read that line in PIECES, exactly the way this one was read.
                // Taking it whole is how "Purdue University, West Lafayette,
                // IN" became the name of a school — and, being a different
                // name from the "Purdue University" read a second earlier, it
                // came back as a SECOND row for the same degree.
                QStringList schoolNamesAbove;
                for (const QString &pieceAbove :
                         piecesOfEntryLine(recentNonEmptyLines.last().trimmed())) {
                    if (looksLikeSchoolName(pieceAbove)) {
                        schoolNamesAbove += institutionNamesIn(withoutPunctuationDebris(pieceAbove));
                    }
                }
                if (schoolNamesAbove.isEmpty()) {
                    schoolNamesAbove = institutionNamesIn(recentNonEmptyLines.last().trimmed());
                }
                educationRecord.schoolName = schoolNamesAbove.first();
                for (int extraIndex = 1; extraIndex < schoolNamesAbove.count(); ++extraIndex) {
                    // No piece index: these came off a different line, so
                    // there are no neighbouring years to claim.
                    additionalSchoolsFound.append({ schoolNamesAbove.at(extraIndex), -1 });
                }
                educationRecord.sourceLineText =
                    recentNonEmptyLines.last() + QStringLiteral("  /  ") + line;
            }

            if (dateMatch.hasMatch()) {
                educationRecord.startDateText = dateMatch.captured(1).trimmed();
                educationRecord.endDateText = dateMatch.captured(2).trimmed();
            } else {
                // Schooling is often stamped with a single graduation year.
                static const QRegularExpression loneYearPattern(yearPattern);
                const QRegularExpressionMatch yearMatch = loneYearPattern.match(line);
                if (yearMatch.hasMatch()) {
                    educationRecord.endDateText = yearMatch.captured(0);
                }
            }

            // Plenty of places people study are not called a university, a
            // college or a school: "Tulsa Technology Center", "Le Cordon
            // Bleu", "Ivy Tech". Under an EDUCATION heading, a plain name
            // sitting beside a credential is where somebody studied — and an
            // empty school box on a certificate is this app reading a degree
            // perfectly and shrugging at a trade.
            if (educationRecord.schoolName.isEmpty() && !credentialPhrase.isEmpty()) {
                for (const QString &piece : piecesOfThisLine) {
                    const bool readsLikeAName =
                        piece.length() >= 3
                        && !piece.contains(credentialPhrase, Qt::CaseInsensitive)
                        && piece.compare(educationRecord.fieldOfStudyText,
                                         Qt::CaseInsensitive) != 0
                        && !looksLikeProse(piece)
                        && !piece.contains(QRegularExpression(QStringLiteral("\\d")))
                        && piece.at(0).isUpper();
                    if (readsLikeAName) {
                        educationRecord.schoolName = withoutPunctuationDebris(piece);
                        break;
                    }
                }
            }

            if (educationRecord.schoolName.isEmpty()
                    && educationRecord.credentialText.isEmpty()) {
                recentNonEmptyLines.append(line);
                continue;
            }

            // A school on one line and its degree on the next are ONE entry,
            // not two. Without this, "Auburn University" followed by "B.S. in
            // Computer Science, 1994" shows the user Auburn twice — once
            // empty — and they have to delete a row the app invented.
            const bool mergesIntoPrevious =
                !parsedInsights.educationRecords.isEmpty()
                && !educationRecord.schoolName.isEmpty()
                && parsedInsights.educationRecords.last().schoolName == educationRecord.schoolName
                && parsedInsights.educationRecords.last().credentialText.isEmpty();

            if (mergesIntoPrevious) {
                EducationRecord &previousRecord = parsedInsights.educationRecords.last();
                previousRecord.credentialText = educationRecord.credentialText;
                previousRecord.fieldOfStudyText = educationRecord.fieldOfStudyText;
                if (previousRecord.endDateText.isEmpty()) {
                    previousRecord.startDateText = educationRecord.startDateText;
                    previousRecord.endDateText = educationRecord.endDateText;
                }
                previousRecord.sourceLineText += QStringLiteral("  /  ") + line;
            } else {
                parsedInsights.educationRecords.append(educationRecord);
            }

            // Each extra school named on that line gets its own row, carrying
            // the school and nothing else. The degree and the dates belonged
            // to the FIRST school; guessing that they belong to a transfer
            // school too would be a confident lie on the user's own history.
            // The row arrives unconfirmed, which is exactly right — this is a
            // guess, and the user is the one who knows.
            for (const SchoolFoundOnThisLine &additionalSchool : additionalSchoolsFound) {
                if (additionalSchool.schoolName.isEmpty()) {
                    continue;
                }
                EducationRecord additionalSchoolRecord;
                additionalSchoolRecord.schoolName = additionalSchool.schoolName;
                additionalSchoolRecord.sourceLineText = line;

                // The years sitting immediately after this school on the line
                // are ITS years. This is the one thing about a second school
                // we can know rather than guess: "Auburn University · 1993 –
                // 1994" puts them right there. Anything further along the
                // line belongs to somebody else and is left alone.
                if (additionalSchool.pieceIndex >= 0) {
                    QStringList yearsRightAfterThisSchool;
                    for (int lookAhead = additionalSchool.pieceIndex + 1;
                         lookAhead < piecesOfThisLine.count(); ++lookAhead) {
                        if (!pieceIsABareYear(piecesOfThisLine.at(lookAhead))) {
                            break;
                        }
                        yearsRightAfterThisSchool.append(piecesOfThisLine.at(lookAhead));
                    }
                    if (yearsRightAfterThisSchool.count() >= 2) {
                        additionalSchoolRecord.startDateText = yearsRightAfterThisSchool.first();
                        additionalSchoolRecord.endDateText = yearsRightAfterThisSchool.at(1);
                    } else if (yearsRightAfterThisSchool.count() == 1) {
                        additionalSchoolRecord.endDateText = yearsRightAfterThisSchool.first();
                    }
                }

                // The DEGREE is not copied across. It belonged to the first
                // school, and putting it on a second one would be a confident
                // lie printed on the user's own history.
                parsedInsights.educationRecords.append(additionalSchoolRecord);
            }
        }

        recentNonEmptyLines.append(line);
        if (recentNonEmptyLines.count() > 4) {
            recentNonEmptyLines.removeFirst();
        }
    }

    finishWorkEntry();
    return parsedInsights;
}
