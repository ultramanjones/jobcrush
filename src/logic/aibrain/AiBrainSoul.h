#pragma once

#include <QString>

// AiBrainSoul
//
// The agent's identity, living in user-editable text files — tuning the
// agent means editing a file, never recompiling. Two files make up the soul:
//
//   primeDirectives.txt — the hard rules. Staged-never-automated is written
//                         here as law; the agent reads it before every task.
//   soul.txt            — the personality and specialization: what Job Crush
//                         is, what ProDocs/packets/the pipeline are, and the
//                         user's voice profile so drafts sound like the user.
//
// On first run both files are created with sensible defaults so the user has
// something real to edit. The assembled text rides as the system prompt on
// every AIBrain request.
//
// A third file, capabilities.txt, belongs to the app, not the user: what Job
// Crush can do today. It is written fresh on every launch, so the brain never
// describes features that changed. It goes last in the assembled text so it
// has the final word.
class AiBrainSoul {
public:
    // folderPath: where the soul files live (under the app's data folder).
    explicit AiBrainSoul(const QString &folderPath);

    // Creates the folder and default files if missing, then reads both.
    // Safe to call again at any time to pick up the user's edits.
    void loadCreatingDefaultsIfMissing();

    // Prime directives, then the soul, then the app's capabilities list,
    // assembled for the system prompt.
    QString assembledSoulText() const;

    // Where the files live, for the "open soul folder" button in Settings.
    QString soulFolderPath() const;

private:
    QString folderPathForSoulFiles;
    QString primeDirectivesText;
    QString soulPersonalityText;
};
