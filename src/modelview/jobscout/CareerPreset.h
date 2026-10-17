#pragma once

#include <QString>
#include <QStringList>

// CareerPreset
//
// A ready-made search profile for one line of work: the job titles to look
// for and the skills that make a posting a good match. Kept as data in a JSON
// file, never in code, so a new career is a new file. Job Crush ships one,
// C++ and Qt (data/career-presets/).
//
// Applying a preset ADDS its titles and skills to the user's own. Nothing the
// user already typed is removed, and every entry can be edited afterwards.
struct CareerPreset {
    QString presetName;
    QString about;
    QStringList jobTitles;
    QStringList strongSkills;   // a posting naming these is a strong match
    QStringList goodSkills;     // these help

    // Reads a preset file. Returns false, with the reason in problemText,
    // when the file is missing or is not a preset.
    static bool readFromFile(const QString &filePath, CareerPreset &presetRead,
                             QString &problemText);
};

// Where the shipped C++ and Qt preset lives inside Job Crush.
inline const QString shippedCppQtCareerPresetPath =
    QStringLiteral(":/data/career-presets/cpp-qt.json");
