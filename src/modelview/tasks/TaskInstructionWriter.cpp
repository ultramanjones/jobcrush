#include "TaskInstructionWriter.h"

namespace {

// Long documents get trimmed instead of sent whole. Past a couple thousand
// characters a posting is mostly boilerplate and legal text. The user pays for
// every character sent.
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
                          .arg(trimmedToLength(briefing.posting.fullDescriptionText, 6000));
    return postingSection;
}

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
                             .arg(trimmedToLength(briefing.careerHistoryText, 4000));
    }
    if (!briefing.professionalDocumentsText.trimmed().isEmpty()) {
        personSection += QStringLiteral("\nTheir documents, in their own words:\n%1\n")
                             .arg(trimmedToLength(briefing.professionalDocumentsText, 8000));
    }
    if (!briefing.userNotesText.trimmed().isEmpty()) {
        personSection += QStringLiteral(
                             "\nTheir own notes on this job. These outrank everything else "
                             "above — they know things the documents do not:\n%1\n")
                             .arg(trimmedToLength(briefing.userNotesText, 2000));
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
                            .arg(trimmedToLength(briefing.extraInstructionText, 1000));
    }

    instructions += housekeepingRules();
    return instructions;
}
