#pragma once

#include <QNetworkAccessManager>

#include "JobSourceProvider.h"

class JobSourceRoster;

// UsaJobsJobSource
//
// Reads USAJOBS, the United States government's own job board.
//
//   GET https://data.usajobs.gov/api/Search?Keyword=nurse&LocationName=Denver
//
// This is the widest source Job Crush has, and the only one that is not aimed
// at software people. Nurses, accountants, park rangers, custodians, air
// traffic controllers, wildland firefighters — every federal opening in the
// country is here.
//
// It needs a free key, which arrives by email from developer.usajobs.gov.
//
// THE THING EVERYONE GETS WRONG: the User-Agent header must be the EMAIL
// ADDRESS the key was registered under. Not a browser string, not the app's
// name. USAJOBS says so plainly in its own documentation, and sending
// anything else is the single most common reason a first request comes back
// refused. That is why Settings asks for the email as well as the key.
class UsaJobsJobSource : public JobSourceProvider {
public:
    // Takes the roster because that is where the key and the email live.
    explicit UsaJobsJobSource(const JobSourceRoster &sourceRoster);

    JobSourceDescriptor descriptor() const override;

    JobScoutReply *searchForJobs(const JobSearchProfile &searchProfile,
                                 QObject *replyParent) override;

private:
    const JobSourceRoster &sourceRoster;
    QNetworkAccessManager networkAccessManager;
};
