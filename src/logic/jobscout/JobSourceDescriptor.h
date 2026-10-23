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

    // Some sites want the email address the key was registered under, sent on
    // every request. USAJOBS is one: it puts that email in the User-Agent
    // header, which is unusual enough that leaving it out is the single most
    // common way people fail to get their first response.
    bool requiresRegisteredEmail = false;

    // Where to go and get the free key. Shown as a link beside the box the
    // key goes in, because "get a key" with no address is a dead end.
    QString accessKeySignupUrl;

    // What to do there, in one line.
    QString accessKeyHelpBlurb;
};

// The catalog of job sites Job Crush knows about — every one of them free to
// use. Paid and subscription boards are deliberately absent from v1.
//
// Adding a site is: write its client, add a block here, set clientIsBuilt.
// Nothing else in the app needs to change, which is the whole point of the
// JobSourceProvider interface.
inline QList<JobSourceDescriptor> jobSourceCatalog()
{
    return {
        { .storageName = QStringLiteral("employerboards"),
          .displayName = QStringLiteral("Companies you follow"),
          .coverageBlurb = QStringLiteral("Every open job at the companies you pick, "
                                          "read from their own hiring system. Add them "
                                          "below by pasting a link to any job there."),
          .officialSiteUrl = QString(),
          .requiresAccessKey = false,
          .clientIsBuilt = true },

        { .storageName = QStringLiteral("remotive"),
          .displayName = QStringLiteral("Remotive"),
          .coverageBlurb = QStringLiteral("Remote roles, heavy on software and tech."),
          .officialSiteUrl = QStringLiteral("https://remotive.com/"),
          .requiresAccessKey = false,
          .clientIsBuilt = true },

        { .storageName = QStringLiteral("arbeitnow"),
          .displayName = QStringLiteral("Arbeitnow"),
          .coverageBlurb = QStringLiteral("Postings pulled straight from company hiring "
                                          "systems, with visa-sponsorship flags."),
          .officialSiteUrl = QStringLiteral("https://www.arbeitnow.com/"),
          .requiresAccessKey = false,
          .clientIsBuilt = true },

        { .storageName = QStringLiteral("jobicy"),
          .displayName = QStringLiteral("Jobicy"),
          .coverageBlurb = QStringLiteral("Remote roles across many fields, with real "
                                          "salary numbers. Checked once an hour, which "
                                          "is what Jobicy asks for."),
          .officialSiteUrl = QStringLiteral("https://jobicy.com/"),
          .requiresAccessKey = false,
          .clientIsBuilt = true },

        { .storageName = QStringLiteral("usajobs"),
          .displayName = QStringLiteral("USAJOBS"),
          .coverageBlurb = QStringLiteral("Every United States federal opening — nurses, "
                                          "accountants, park rangers, air traffic "
                                          "controllers, not just desk work."),
          .officialSiteUrl = QStringLiteral("https://www.usajobs.gov/"),
          .requiresAccessKey = true,
          .clientIsBuilt = true,
          .requiresRegisteredEmail = true,
          .accessKeySignupUrl = QStringLiteral("https://developer.usajobs.gov/apirequest/"),
          .accessKeyHelpBlurb = QStringLiteral("Free, and it arrives by email. USAJOBS "
                                               "needs the same email address you sign up "
                                               "with, so put both in below.") },

        { .storageName = QStringLiteral("remoteok"),
          .displayName = QStringLiteral("RemoteOK"),
          .coverageBlurb = QStringLiteral("Remote-only board, long-running and broad."),
          .officialSiteUrl = QStringLiteral("https://remoteok.com/"),
          .requiresAccessKey = false,
          .clientIsBuilt = false },

        { .storageName = QStringLiteral("themuse"),
          .displayName = QStringLiteral("The Muse"),
          .coverageBlurb = QStringLiteral("Company-profile driven listings across many "
                                          "industries."),
          .officialSiteUrl = QStringLiteral("https://www.themuse.com/"),
          .requiresAccessKey = false,
          .clientIsBuilt = false },

        { .storageName = QStringLiteral("adzuna"),
          .displayName = QStringLiteral("Adzuna"),
          .coverageBlurb = QStringLiteral("Large general aggregate across many countries."),
          .officialSiteUrl = QStringLiteral("https://www.adzuna.com/"),
          .requiresAccessKey = true,
          .clientIsBuilt = false },

        { .storageName = QStringLiteral("jooble"),
          .displayName = QStringLiteral("Jooble"),
          .coverageBlurb = QStringLiteral("Worldwide aggregate covering a lot of ground."),
          .officialSiteUrl = QStringLiteral("https://jooble.org/"),
          .requiresAccessKey = true,
          .clientIsBuilt = false },
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
