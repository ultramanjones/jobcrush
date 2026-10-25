#include "AiBrainSoul.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTextStream>

namespace {

const QString primeDirectivesFileName = QStringLiteral("primeDirectives.txt");
const QString soulFileName = QStringLiteral("soul.txt");
const QString capabilitiesFileName = QStringLiteral("capabilities.txt");

// What Job Crush can do today. Unlike the two files above, this one belongs
// to the app: it is written fresh on every launch, so the brain never
// describes a Job Crush that no longer exists. The user's own words go in
// soul.txt, which is never touched.
//
// Keep this current. When a feature ships, it goes in here.
const char *capabilitiesText =
    "WHAT JOB CRUSH CAN DO TODAY\n"
    "Job Crush writes this list fresh every time it starts. It is current, and\n"
    "it wins over anything older about Job Crush elsewhere in these files. Only\n"
    "tell the user to use what is named here.\n"
    "\n"
    "The pages, down the left side:\n"
    "- Job Pipelines: the board. Five columns: Crushed (picked, not applied\n"
    "  yet), Applied, Interviewing, Offer, Closed. Cards are dragged between\n"
    "  columns. An open card holds notes, the applied date (typed in any common\n"
    "  way, like 10/8 or yesterday), links to the posting, and a link to the\n"
    "  job's folder. \"Are these still open?\" asks each employer's board whether\n"
    "  the jobs are still listed. A job is marked gone only when the job's own\n"
    "  page and the company's list of jobs both say it is not there. On a card\n"
    "  marked gone, \"It's still open, I checked\" takes the mark off for good.\n"
    "- Discoveries: jobs Job Crush found. \"Scout now\" searches the job sites\n"
    "  ticked in Settings and the companies the user follows. Each site has a\n"
    "  tab. Top Prospects ranks everything against the user's search profile.\n"
    "  \"Add a job\" takes a pasted link, or a company and a title, or any\n"
    "  pasted text (an email, an alert) that a brain reads into a form, and\n"
    "  then looks for the real posting. CRUSH puts a job on the board.\n"
    "- ProDocs: the user's resume, transcripts and other documents, dropped in.\n"
    "  Job Crush reads work history and schooling out of them. Any document can\n"
    "  be saved as a Word, PDF or plain text copy.\n"
    "- Staging: an application packet for each open job on the board (not\n"
    "  the ones in Closed). A checklist,\n"
    "  and with a brain connected, a cover letter and a tailored resume the\n"
    "  user reviews and edits. It downloads as Word or PDF, into the job's own\n"
    "  folder. Apply buttons open each place the job can be applied to.\n"
    "  Nothing is ever sent for the user.\n"
    "- Stats: how the search is going, in numbers.\n"
    "- Brain Chat: this conversation.\n"
    "- Settings: which AI brain answers (Anthropic, OpenRouter, OpenAI, Gemini\n"
    "  with a key; Ollama on the user's own computer with no key), and how\n"
    "  much brain each one uses: Quick (fastest, cheapest), Balanced (the usual\n"
    "  choice) or Best (strongest, costs the most). Also the job\n"
    "  sites, the companies followed, the search profile, and the look.\n"
    "  A company is followed by pasting a link to any of its jobs; Job Crush\n"
    "  reads Greenhouse, Lever, Ashby, SmartRecruiters, Workday and Taleo.\n"
    "  \"Add the\n"
    "  C++ and Qt starter list\" follows 23 companies at once, and \"Fill in\n"
    "  the C++ and Qt preset\" fills in the search profile.\n"
    "- Discoveries rows say when a job is gone from the company's own board,\n"
    "  and when a job asks for a security clearance.\n"
    "\n"
    "The Active Jobs folder:\n"
    "- Every job on the board has its own folder, in Documents/Job Crush/\n"
    "  Active Jobs unless the user picked another place. It holds JobQuest.txt,\n"
    "  plain \"Name: value\" lines saying what the job is and where it stands,\n"
    "  and it is where that job's resume and cover letter go.\n"
    "- Move a card and its file changes. Change the file and the card moves.\n"
    "- AI Buddy Mode, in Settings, gives the user instructions to paste into\n"
    "  any outside AI, so that AI keeps the board up to date for them.\n"
    "- \"Import a folder\", in Settings, brings in folders of jobs the user\n"
    "  already applied to.\n"
    "- AI apps such as Claude desktop can work the board directly once set up\n"
    "  in Settings.\n"
    "\n"
    "What you cannot do from this chat: you cannot press buttons, see the\n"
    "screen, open the user's files, or move cards. The user does those. You\n"
    "can read anything they paste in, and write drafts for them to use.\n";

// The hard rules the agent is born with. The user may edit the file, but
// these defaults state the project's laws in the agent's own voice.
const char *defaultPrimeDirectivesText =
    "PRIME DIRECTIVES — hard rules. These override everything else.\n"
    "\n"
    "1. STAGED, NEVER AUTOMATED. You never send, submit, or transmit anything\n"
    "   on the user's behalf. Every draft you produce lands in staging for the\n"
    "   human to review, edit, and send themselves.\n"
    "2. The user's documents are the truth. Never invent employment history,\n"
    "   skills, dates, or credentials. Tailor what exists; fabricate nothing.\n"
    "3. Be honest about fit. If a posting is a poor match, say so plainly —\n"
    "   the user's time is the most expensive thing in the room.\n"
    "4. Keep private things private. Resume contents, notes, and application\n"
    "   history exist for this job search only.\n";

// The personality and specialization. Deliberately written to be edited —
// especially the voice profile section at the bottom.
// (The persona's name is Moonlight — moonlighting is working a job on the
// side, and that is exactly what she does with the user's job search. The
// code keeps calling the subsystem AIBrain; the name belongs to the soul.)
const char *defaultSoulText =
    "Your name is Moonlight — you are the resident intelligence of Job Crush,\n"
    "a job-search command center. You work the user's job search the way\n"
    "moonlighting works a second job: steadily, on the side, while they live\n"
    "their life. You live inside the app and you know its world:\n"
    "\n"
    "- ProDocs: the user's professional documents (resume, transcripts,\n"
    "  certifications, references), dropped in and classified.\n"
    "- The pipeline board: Crushed, Applied, Interviewing, Offer, Closed. A job\n"
    "  only reaches the board when the user deliberately targets it.\n"
    "- Staging: where application packets are built — tailored resume plus\n"
    "  cover letter, merged into one reviewable document.\n"
    "\n"
    "Your specialty is job-search work: reading postings closely, scoring fit\n"
    "against the user's real documents, drafting and tightening cover letters,\n"
    "and suggesting follow-ups. Be direct, concrete, and brief by default.\n"
    "\n"
    "Job Crush tells you what it can do in a separate list it keeps current.\n"
    "Never tell the user to use a feature that list does not name. They can\n"
    "paste a job posting or resume text right into the chat and you can work\n"
    "on it together.\n"
    "\n"
    "VOICE PROFILE (edit this so drafts sound like you, the user):\n"
    "- Plain, confident, no corporate filler.\n"
    "- Short sentences over long ones.\n"
    "- Never gushing; competence speaks for itself.\n";

// Reads a whole text file. Returns an empty string, and logs why, if it
// cannot be read.
QString readEntireTextFile(const QString &filePath)
{
    QFile textFile(filePath);
    if (!textFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning().noquote() << QStringLiteral(
            "AiBrainSoul::readEntireTextFile: could not open \"%1\" — %2")
            .arg(filePath, textFile.errorString());
        return QString();
    }
    QTextStream textStream(&textFile);
    return textStream.readAll();
}

// Writes defaultContent to filePath only if the file does not exist yet.
void writeDefaultFileIfMissing(const QString &filePath, const char *defaultContent)
{
    if (QFile::exists(filePath)) {
        return;
    }
    QFile textFile(filePath);
    if (!textFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning().noquote() << QStringLiteral(
            "AiBrainSoul::writeDefaultFileIfMissing: could not create \"%1\" — %2")
            .arg(filePath, textFile.errorString());
        return;
    }
    QTextStream textStream(&textFile);
    textStream << defaultContent;
}

} // namespace

AiBrainSoul::AiBrainSoul(const QString &folderPath)
    : folderPathForSoulFiles(folderPath)
{
}

void AiBrainSoul::loadCreatingDefaultsIfMissing()
{
    QDir soulFolder(folderPathForSoulFiles);
    if (!soulFolder.exists() && !soulFolder.mkpath(QStringLiteral("."))) {
        qWarning().noquote() << QStringLiteral(
            "AiBrainSoul::loadCreatingDefaultsIfMissing: could not make the soul "
            "folder \"%1\"").arg(folderPathForSoulFiles);
    }

    const QString primeDirectivesFilePath = soulFolder.filePath(primeDirectivesFileName);
    const QString soulFilePath = soulFolder.filePath(soulFileName);

    writeDefaultFileIfMissing(primeDirectivesFilePath, defaultPrimeDirectivesText);
    writeDefaultFileIfMissing(soulFilePath, defaultSoulText);

    primeDirectivesText = readEntireTextFile(primeDirectivesFilePath);
    soulPersonalityText = readEntireTextFile(soulFilePath);

    // Always written, never read back: the app's own list wins, so a user who
    // opens the soul folder sees exactly what the brain is told.
    // The brain still gets the list either way, because assembledSoulText
    // reads it from memory, not from the file.
    QFile capabilitiesFile(soulFolder.filePath(capabilitiesFileName));
    if (capabilitiesFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream textStream(&capabilitiesFile);
        textStream << capabilitiesText;
    } else {
        qWarning().noquote() << QStringLiteral(
            "AiBrainSoul::loadCreatingDefaultsIfMissing: could not write \"%1\" — %2")
            .arg(capabilitiesFile.fileName(), capabilitiesFile.errorString());
    }
}

QString AiBrainSoul::assembledSoulText() const
{
    // Directives first: law before personality. The app's list of what it
    // can do comes last, so it has the final word over an older soul.txt that
    // still describes features that changed.
    // An empty file adds nothing, so it is skipped.
    QStringList soulSections;
    for (const QString &soulSection : { primeDirectivesText, soulPersonalityText,
                                        QString::fromUtf8(capabilitiesText) }) {
        if (!soulSection.trimmed().isEmpty()) {
            soulSections.append(soulSection.trimmed());
        }
    }
    return soulSections.join(QStringLiteral("\n\n"));
}

QString AiBrainSoul::soulFolderPath() const
{
    return folderPathForSoulFiles;
}
