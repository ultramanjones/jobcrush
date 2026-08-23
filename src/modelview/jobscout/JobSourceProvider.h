#pragma once

#include <QString>

#include "JobSourceDescriptor.h"

class JobScoutReply;
class JobSearchProfile;
class QObject;

// JobSourceProvider
//
// The interface every job site client implements — the JobScout twin of
// AiBrainProvider, and intentionally the same shape, so a developer who has
// read one has read both.
//
// A source is a thin HTTPS client speaking one site's public JSON API. It
// holds no state, stores nothing, and knows nothing about the database, the
// user's pipeline, or scoring. It fetches, translates the site's JSON into
// JobPosting structs, and hands them back.
//
// Job Crush talks to documented APIs whose owners publish them for this
// purpose. It does not scrape sites that forbid it — a public portfolio repo
// that broke a job board's terms would be worse than useless.
class JobSourceProvider {
public:
    virtual ~JobSourceProvider() = default;

    // Who this source is, for the Settings list and the Discoveries tab.
    virtual JobSourceDescriptor descriptor() const = 0;

    // Starts one sweep and returns immediately. The returned reply (parented
    // to replyParent) delivers the found postings, or an honest failure.
    //
    // The profile shapes the request as far as the site allows. Sites that
    // accept no search terms return their latest listings and JobScout ranks
    // them locally instead — a site with no search box is still worth having.
    virtual JobScoutReply *searchForJobs(const JobSearchProfile &searchProfile,
                                         QObject *replyParent) = 0;
};
