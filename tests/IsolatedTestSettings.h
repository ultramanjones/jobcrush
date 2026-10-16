#pragma once

#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>

// IsolatedTestSettings
//
// JobSearchProfile saves itself through QSettings. On Windows that is the
// registry, under the app's own name. A test must never write into the real
// user's settings, so every test that touches a profile calls this first.
//
// It gives the test its own app name and an INI file in a temporary folder
// that is deleted when the test ends.
inline void useIsolatedTestSettings(QTemporaryDir &settingsFolder)
{
    QCoreApplication::setOrganizationName(QStringLiteral("JobCrushTests"));
    QCoreApplication::setApplicationName(QStringLiteral("JobCrushTests"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsFolder.path());
    QSettings().clear();
}
