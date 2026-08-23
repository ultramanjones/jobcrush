#include "ResumeInsightParser.h"

#include <QRegularExpression>

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
    return cleanedText.simplified();
}

QStringList piecesOfEntryLine(const QString &lineRemainder)
{
    static const QRegularExpression separatorPattern(
        QStringLiteral("\\s+[–—|·•]\\s+|\\s+-\\s+|\\s+at\\s+|,\\s+|\\t+"),
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
    return piece.length() > 60 || piece.endsWith(QLatin1Char('.'));
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

bool looksLikeSchoolName(const QString &piece)
{
    static const QStringList schoolWords = {
        QStringLiteral("university"), QStringLiteral("college"), QStringLiteral("institute"),
        QStringLiteral("academy"), QStringLiteral("school"), QStringLiteral("polytechnic"),
        QStringLiteral("seminary"), QStringLiteral("conservatory"),
        // "Univ Alabama Huntsville" is how it appears on a real transcript;
        // matching only the full word would have missed it entirely.
        QStringLiteral("univ"),
    };
    const QString loweredPiece = piece.toLower();
    for (const QString &schoolWord : schoolWords) {
        if (loweredPiece.contains(schoolWord)) {
            return true;
        }
    }
    return false;
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

    static const QRegularExpression leadInPattern(
        QStringLiteral("^(?:in|of|,|-|–|—|:)\\s*"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch leadIn = leadInPattern.match(tail);
    if (!leadIn.hasMatch()) {
        return QString();
    }
    tail = tail.mid(leadIn.capturedLength()).trimmed();

    // Stop at the next separator — anything past it is the school or a date.
    static const QRegularExpression tailStopPattern(QStringLiteral("\\s*[,|–—•]|\\s+-\\s+"));
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
    return tail;
}

} // namespace

ParsedResumeInsights ResumeInsightParser::parseResumeText(const QString &resumeText) const
{
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

                for (const QString &piece : pieces) {
                    QString splitEmployer;
                    QString splitRole;
                    if (workEntryBeingBuilt.employerName.isEmpty()
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

            for (const QString &piece : piecesOfEntryLine(line)) {
                if (looksLikeSchoolName(piece) && educationRecord.schoolName.isEmpty()) {
                    QString schoolNamePiece = piece;

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

                    educationRecord.schoolName = withoutPunctuationDebris(schoolNamePiece);
                }
            }
            // The credential and the school often sit on consecutive lines.
            if (educationRecord.schoolName.isEmpty() && !recentNonEmptyLines.isEmpty()
                    && looksLikeSchoolName(recentNonEmptyLines.last())) {
                educationRecord.schoolName = recentNonEmptyLines.last().trimmed();
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
        }

        recentNonEmptyLines.append(line);
        if (recentNonEmptyLines.count() > 4) {
            recentNonEmptyLines.removeFirst();
        }
    }

    finishWorkEntry();
    return parsedInsights;
}
