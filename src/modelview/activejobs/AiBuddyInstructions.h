#pragma once

#include <QDir>
#include <QString>

// AiBuddyInstructions
//
// The text a user pastes into any outside AI (ChatGPT, Claude, Gemini) once,
// so that AI keeps Job Crush up to date for them.
//
// It names the user's own Active Jobs folder and lists every line of
// JobQuest.txt and every Status word, because an AI that has to guess the
// format will guess wrong. Kept in one place so the format written here and
// the format JobQuestFile reads can be checked against each other.
namespace AiBuddyInstructions {

inline QString instructionsFor(const QString &activeJobsFolderPath)
{
    return QStringLiteral(
        "I track my job search in an app called Job Crush. Please keep it up to "
        "date for me whenever you help me find or apply to a job.\n"
        "\n"
        "My job folder is:\n"
        "%1\n"
        "\n"
        "For each job:\n"
        "1. Make one folder for the job inside my job folder. Name it "
        "\"Company - Title\", for example \"Acme Robotics - Senior Qt Developer\".\n"
        "2. In that folder, save a plain text file named JobQuest.txt. Write one "
        "\"Name: value\" per line, like this:\n"
        "\n"
        "Quest created by: (your name, for example ChatGPT or Claude)\n"
        "Company: Acme Robotics\n"
        "Title: Senior Qt Developer\n"
        "Link: (the web address of the job posting)\n"
        "Found on: (the site, for example FlexJobs)\n"
        "Status: Crushed\n"
        "Applied on: (the day I sent it, written like 10/14/2026)\n"
        "Pay: (the pay, as the posting says it)\n"
        "Where: (city and state, or Remote)\n"
        "Notes: (anything I should remember)\n"
        "\n"
        "3. Only Company and Title are required. Leave out any line you do not "
        "know. Never guess a value.\n"
        "4. Status must be one of these words:\n"
        "   Crushed: I want this job but have not applied yet.\n"
        "   Applied: I sent the application.\n"
        "   Interviewing: an interview is scheduled.\n"
        "   Offer: they made me an offer.\n"
        "   Closed: it is over. They said no, I withdrew, or I turned it down.\n"
        "5. Save any resume or cover letter you write for a job in that job's "
        "folder.\n"
        "6. When something changes, edit the lines in that job's JobQuest.txt. "
        "Never make a second folder for the same job. Job Crush also writes the "
        "changes I make in the app into this file, so read the file before you "
        "change it.\n"
        "7. If you cannot save files on my computer, write out the JobQuest.txt "
        "text for me and tell me which folder to save it in.\n")
        .arg(QDir::toNativeSeparators(activeJobsFolderPath));
}

} // namespace AiBuddyInstructions
