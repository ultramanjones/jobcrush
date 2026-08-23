#pragma once

#include <QList>
#include <QString>

// JobSourceDescriptor
//
// Everything Job Crush knows ABOUT a job site, whether or not its client has
// been written yet. Pure data.
//
// The Settings page lists every descriptor in the catalog below, so the user
// can see the whole landscape at once — the same honesty the AIBrain provider
// tabs use: a site that isn't wired up yet says so out loud instead of being
// hidden until some future release surprises them with it.
struct JobSourceDescriptor {
    QString storageName;        // "remotive" — the id used in settings and the database
    QString displayName;        // "Remotive" — what a human reads
    QString coverageBlurb;      // one line: what kind of jobs live here
    QString officialSiteUrl;    // the site's own front door
    bool requiresAccessKey = false;  // does the user have to register for a free key?
    bool clientIsBuilt = false;      // has Job Crush learned to talk to it yet?
};

// The catalog of job sites Job Crush knows about — every one of them free to
// use. Paid and subscription boards are deliberately absent from v1.
//
// Adding a site is: write its client, add a line here, flip clientIsBuilt.
// Nothing else in the app needs to change, which is the whole point of the
// JobSourceProvider interface.
inline QList<JobSourceDescriptor> jobSourceCatalog()
{
    return {
        { QStringLiteral("remotive"),
          QStringLiteral("Remotive"),
          QStringLiteral("Remote roles, heavy on software and tech."),
          QStringLiteral("https://remotive.com/"),
          false, true },

        { QStringLiteral("arbeitnow"),
          QStringLiteral("Arbeitnow"),
          QStringLiteral("Postings pulled straight from company hiring systems, "
                         "with visa-sponsorship flags."),
          QStringLiteral("https://www.arbeitnow.com/"),
          false, true },

        { QStringLiteral("remoteok"),
          QStringLiteral("RemoteOK"),
          QStringLiteral("Remote-only board, long-running and broad."),
          QStringLiteral("https://remoteok.com/"),
          false, false },

        { QStringLiteral("themuse"),
          QStringLiteral("The Muse"),
          QStringLiteral("Company-profile driven listings across many industries."),
          QStringLiteral("https://www.themuse.com/"),
          false, false },

        { QStringLiteral("usajobs"),
          QStringLiteral("USAJOBS"),
          QStringLiteral("Every United States federal government opening."),
          QStringLiteral("https://www.usajobs.gov/"),
          true, false },

        { QStringLiteral("adzuna"),
          QStringLiteral("Adzuna"),
          QStringLiteral("Large general aggregate across many countries."),
          QStringLiteral("https://www.adzuna.com/"),
          true, false },

        { QStringLiteral("jooble"),
          QStringLiteral("Jooble"),
          QStringLiteral("Worldwide aggregate covering a lot of ground."),
          QStringLiteral("https://jooble.org/"),
          true, false },
    };
}

// The descriptor for one storage name. found is set accordingly.
inline JobSourceDescriptor jobSourceDescriptorFor(const QString &storageName, bool &found)
{
    for (const JobSourceDescriptor &descriptor : jobSourceCatalog()) {
        if (descriptor.storageName == storageName) {
            found = true;
            return descriptor;
        }
    }
    found = false;
    return JobSourceDescriptor();
}
