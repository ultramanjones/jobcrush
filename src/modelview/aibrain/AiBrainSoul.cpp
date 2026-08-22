#include "AiBrainSoul.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

namespace {

const QString primeDirectivesFileName = QStringLiteral("primeDirectives.txt");
const QString soulFileName = QStringLiteral("soul.txt");

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
    "- The pipeline board: Saved, Applied, Interview, Offer, Closed. A job\n"
    "  only reaches the board when the user deliberately targets it.\n"
    "- Staging: where application packets are built — tailored resume plus\n"
    "  cover letter, merged into one reviewable document.\n"
    "\n"
    "Your specialty is job-search work: reading postings closely, scoring fit\n"
    "against the user's real documents, drafting and tightening cover letters,\n"
    "and suggesting follow-ups. Be direct, concrete, and brief by default.\n"
    "\n"
    "Job Crush is rolling out in phases, and some tabs you know about may not\n"
    "be built yet (they show a phase number in the sidebar). Never instruct\n"
    "the user to use a feature they may not have. Help with what is always\n"
    "here: this conversation. They can paste a job posting or resume text\n"
    "right into the chat and you can work on it together, today.\n"
    "\n"
    "VOICE PROFILE (edit this so drafts sound like you, the user):\n"
    "- Plain, confident, no corporate filler.\n"
    "- Short sentences over long ones.\n"
    "- Never gushing; competence speaks for itself.\n";

// Reads a whole text file; returns an empty string if it cannot be read.
QString readEntireTextFile(const QString &filePath)
{
    QFile textFile(filePath);
    if (!textFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
    if (textFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream textStream(&textFile);
        textStream << defaultContent;
    }
}

} // namespace

AiBrainSoul::AiBrainSoul(const QString &folderPath)
    : folderPathForSoulFiles(folderPath)
{
}

void AiBrainSoul::loadCreatingDefaultsIfMissing()
{
    QDir soulFolder(folderPathForSoulFiles);
    if (!soulFolder.exists()) {
        soulFolder.mkpath(QStringLiteral("."));
    }

    const QString primeDirectivesFilePath = soulFolder.filePath(primeDirectivesFileName);
    const QString soulFilePath = soulFolder.filePath(soulFileName);

    writeDefaultFileIfMissing(primeDirectivesFilePath, defaultPrimeDirectivesText);
    writeDefaultFileIfMissing(soulFilePath, defaultSoulText);

    primeDirectivesText = readEntireTextFile(primeDirectivesFilePath);
    soulPersonalityText = readEntireTextFile(soulFilePath);
}

QString AiBrainSoul::assembledSoulText() const
{
    // Directives first — law before personality.
    QString assembledText = primeDirectivesText;
    if (!assembledText.isEmpty() && !soulPersonalityText.isEmpty()) {
        assembledText += QStringLiteral("\n\n");
    }
    assembledText += soulPersonalityText;
    return assembledText;
}

QString AiBrainSoul::soulFolderPath() const
{
    return folderPathForSoulFiles;
}
