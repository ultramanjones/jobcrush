#include "TaskInstructionWriter.h"

namespace {

// The most characters of each part of the briefing that are sent. The user
// pays for every character sent, so each part stops where more text stops
// helping. Past a few thousand characters a posting is mostly boilerplate and
// legal text. The documents get the most room, because the drafts are built
// from them.
constexpr int longestPostingLength = 6000;
constexpr int longestCareerHistoryLength = 4000;
constexpr int longestDocumentsLength = 8000;
constexpr int longestUserNotesLength = 2000;
constexpr int longestExtraInstructionLength = 1000;

// Long documents get trimmed instead of sent whole, with a line that says so,
// so the AI does not treat a cut-off text as complete.
QString trimmedToLength(const QString &text, int longestUsefulLength)
{
    const QString tidyText = text.trimmed();
    if (tidyText.length() <= longestUsefulLength) {
        return tidyText;
    }
    return tidyText.left(longestUsefulLength)
         + QStringLiteral("\n\n[trimmed — the rest of this document was not sent]");
}

} // namespace

// The facts about the job, then the posting text itself. Location and pay are
// left out when the posting did not give them, so the AI is not handed an
// empty line to fill.
QString TaskInstructionWriter::postingSectionOf(const TaskBriefing &briefing) const
{
    QString postingSection = QStringLiteral("THE JOB\n");
    postingSection += QStringLiteral("Company: %1\n").arg(briefing.posting.companyName);
    postingSection += QStringLiteral("Title: %1\n").arg(briefing.posting.positionTitle);
    if (!briefing.posting.locationText.isEmpty()) {
        postingSection += QStringLiteral("Location: %1\n").arg(briefing.posting.locationText);
    }
    if (!briefing.posting.salaryText.isEmpty()) {
        postingSection += QStringLiteral("Pay as posted: %1\n").arg(briefing.posting.salaryText);
    }
    postingSection += QStringLiteral("\nThe posting, as published:\n%1\n")
                          .arg(trimmedToLength(briefing.posting.fullDescriptionText,
                                               longestPostingLength));
    return postingSection;
}

// What Job Crush knows about the user: history, documents, and their notes on
// this job. With nothing on file, the AI is told so and told not to make
// anything up.
QString TaskInstructionWriter::personSectionOf(const TaskBriefing &briefing) const
{
    QString personSection = QStringLiteral("\nTHE PERSON APPLYING\n");

    if (briefing.careerHistoryText.trimmed().isEmpty()
            && briefing.professionalDocumentsText.trimmed().isEmpty()) {
        personSection += QStringLiteral(
            "Job Crush has nothing on file about this person yet — no resume, no "
            "work history. Do not fill the gap with plausible-sounding experience. "
            "Say plainly what you would need in order to do this properly.\n");
        return personSection;
    }

    if (!briefing.careerHistoryText.trimmed().isEmpty()) {
        personSection += QStringLiteral("\nTheir history, as read from their own documents:\n%1\n")
                             .arg(trimmedToLength(briefing.careerHistoryText,
                                                  longestCareerHistoryLength));
    }
    if (!briefing.professionalDocumentsText.trimmed().isEmpty()) {
        personSection += QStringLiteral("\nTheir documents, in their own words:\n%1\n")
                             .arg(trimmedToLength(briefing.professionalDocumentsText,
                                                  longestDocumentsLength));
    }
    if (!briefing.userNotesText.trimmed().isEmpty()) {
        personSection += QStringLiteral(
                             "\nTheir own notes on this job. These outrank everything else "
                             "above — they know things the documents do not:\n%1\n")
                             .arg(trimmedToLength(briefing.userNotesText,
                                                  longestUserNotesLength));
    }
    return personSection;
}

QString TaskInstructionWriter::housekeepingRules() const
{
    return QStringLiteral(
        "\nHOW TO ANSWER\n"
        "Reply with the finished piece and nothing else. No preamble, no sign-off "
        "to the reader of this instruction, no explanation of what you did — what "
        "you send back gets stored as the document itself, so anything "
        "conversational ends up printed on the page an employer reads.\n"
        "Markdown for structure (## headings, - bullets, **bold**). Nothing else.\n"
        "Never state a qualification, employer, date or credential that is not in "
        "the material above. If something important is missing, leave a clearly "
        "marked [square-bracket gap] for the person to fill in — a gap they can "
        "see beats a fact you invented.\n");
}

// The instruction is built in this order: what this task is, the job, the
// person, what the user typed for this run, and last the rules for the
// answer's shape. The user's own words come after the built-in task text, and
// say they win, so a conflict is settled in the user's favor.
QString TaskInstructionWriter::instructionsFor(AiBrainTaskKind taskKind,
                                               const TaskBriefing &briefing) const
{
    QString instructions;

    switch (taskKind) {
    case AiBrainTaskKind::ParsePosting:
        instructions = QStringLiteral(
            "Read this job posting and write it back in plain words, for somebody "
            "deciding whether to spend an evening applying.\n\n"
            "Use these headings exactly: ## What the job is, ## What they're asking "
            "for, ## What they're offering, ## Worth knowing.\n"
            "Under \"Worth knowing\" put the things a posting buries: shift patterns, "
            "travel, on-call, security clearance, a degree stated as required, an "
            "agency posting on someone else's behalf. If the posting is vague about "
            "pay or location, say so — that IS worth knowing.\n"
            "Keep it under 250 words. Somebody with thirty tabs open is reading it.\n");
        break;

    case AiBrainTaskKind::ScoreFit:
        instructions = QStringLiteral(
            "Judge how well this person's actual history matches this job.\n\n"
            "FIRST LINE, exactly this shape and nothing else on it:\n"
            "FIT: <number 0-100>\n\n"
            "Then, under ## Where you match, the specific things in their history "
            "that line up — name them, with the employer or the credential they come "
            "from. Then, under ## Where you don't, the gaps, plainly. Then ## Worth "
            "saying anyway: how to handle the biggest gap honestly, if it can be "
            "handled.\n\n"
            "Be honest. A 40 that explains itself is more useful than a 75 that "
            "wastes their week. But score the whole person. Someone who has done "
            "the work for ten years and never finished the degree is not a 20 "
            "because the posting says a bachelor's is required.\n"
            "Under 250 words after the first line.\n");
        break;

    case AiBrainTaskKind::DraftCoverLetter:
        instructions = QStringLiteral(
            "Write a cover letter for this job, in this person's own voice.\n\n"
            "Their voice, not yours: match the way they write in their own "
            "documents. No \"I am writing to express my keen interest\", no "
            "\"leverage\", no \"passionate about\", no three-word rule-of-three "
            "flourishes. Short sentences are fine. A plain one is better than a "
            "polished one.\n"
            "Three or four paragraphs. Open with why this job, specifically — "
            "something that is actually in the posting, not a compliment about the "
            "company. Middle: two or three things they have genuinely done that "
            "matter here, with the specifics that make them believable. Close "
            "briefly, without begging.\n"
            "No heading, no address block, no date — those are added when it is "
            "exported. Start at \"Dear ...\" and if the posting does not name a "
            "person, use \"Dear Hiring Team,\".\n"
            "Leave a BLANK LINE before their name at the end. A single line break "
            "is a soft break in markdown and their name would be exported sitting on "
            "the same line as the sign-off.\n"
            "Under 300 words.\n");
        break;

    case AiBrainTaskKind::TailorResume:
        instructions = QStringLiteral(
            "Rewrite this person's resume so it aims at this one job.\n\n"
            "You are REARRANGING AND REPHRASING WHAT IS THERE. You are not adding "
            "experience, not adjusting dates, not promoting anybody, not turning six "
            "months into a year. Every line has to be traceable to the material "
            "above.\n"
            "What you may do: lead with the experience this posting cares about, cut "
            "what it does not, use the words the posting uses where they honestly "
            "describe the same work, and turn duty lists into what they actually "
            "achieved where their own documents say what that was.\n"
            "Structure: ## Summary (three lines at most), ## Experience (employer, "
            "title, dates, then bullets), ## Education, ## Skills. Keep dates exactly "
            "as their documents write them.\n");
        break;

    case AiBrainTaskKind::SuggestFollowUp:
        instructions = QStringLiteral(
            "Work out what this person should do after they send this application.\n\n"
            "## When to follow up — a specific number of days, and why that number "
            "for this employer.\n"
            "## What to say — a short message they can actually send, in their voice, "
            "that gives the employer a reason to reply rather than just asking for "
            "news.\n"
            "## If you hear nothing — one honest line about when to let it go.\n"
            "Under 200 words. Nothing here gets sent by Job Crush; it is theirs to "
            "send when they choose.\n");
        break;
    }

    instructions += QStringLiteral("\n") + postingSectionOf(briefing);
    instructions += personSectionOf(briefing);

    if (!briefing.extraInstructionText.trimmed().isEmpty()) {
        instructions += QStringLiteral(
                            "\nWHAT THEY ASKED YOU FOR, in their own words. This wins over "
                            "any instruction above it that it contradicts:\n%1\n")
                            .arg(trimmedToLength(briefing.extraInstructionText,
                                                 longestExtraInstructionLength));
    }

    instructions += housekeepingRules();
    return instructions;
}
