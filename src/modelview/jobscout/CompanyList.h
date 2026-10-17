#pragma once

#include <QList>
#include <QString>

// CompanyListEntry
//
// One company on a starter list. Plain data.
struct CompanyListEntry {
    QString companyName;
    QString sector;          // "Medical devices", "Defense"…
    QString hiringSystem;    // an AtsBoardName, or empty when not yet known
    QString followLink;      // a link FollowedEmployerRoster can follow, or empty
    QString lastChecked;     // the day the link was checked against the live feed
    QString note;
};

// CompanyList
//
// A list of companies, kept as data in a JSON file, never in code, so it can
// be changed without rebuilding Job Crush. Job Crush ships one: C++ and Qt
// employers in medical devices, defense, automotive and tech
// (data/company-lists/). It is a starting point, not a limit: the user adds
// the list once and then removes or adds companies in Settings like any other.
struct CompanyList {
    QString listName;
    QString about;
    QList<CompanyListEntry> companies;

    // Reads a list file. Returns false, with the reason in problemText, when
    // the file is missing or is not a list.
    static bool readFromFile(const QString &filePath, CompanyList &listRead,
                             QString &problemText);

    // The entry for a company, matched loosely: case, punctuation, words
    // like "Inc" and anything in brackets are ignored, so "Collins Aerospace"
    // finds "Collins Aerospace (RTX)". Returns false when it is not listed.
    bool findCompany(const QString &companyName, CompanyListEntry &entryFound) const;
};

// Where the shipped C++ and Qt list lives inside Job Crush.
inline const QString shippedCppQtCompanyListPath =
    QStringLiteral(":/data/company-lists/cpp-qt-medtech-tech.json");
