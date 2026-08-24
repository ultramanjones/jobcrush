#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>

#include "JobSourceProvider.h"

// JobicyJobSource
//
// Reads Jobicy's public job feed.
//
//   GET https://jobicy.com/api/v2/remote-jobs?count=50&tag=<what you want>
//
// No key, no sign-up, nothing to put in Settings and nothing to leak. Every
// job on it is remote.
//
// Jobicy's terms are the friendliest of any source Job Crush uses: it may
// build its own lists, summaries and categories out of the feed, so long as
// Jobicy is credited and the original link is kept. Job Crush does both — the
// Discoveries row says where the job came from, and the CRUSH button opens
// Jobicy's own page rather than a copy.
//
// Jobicy asks for no more than one check an hour. That is not a suggestion
// Job Crush ignores, so this class refuses to ask again inside the hour and
// says why. The jobs from the last check are already stored, so nothing is
// lost by waiting.
class JobicyJobSource : public JobSourceProvider {
public:
    JobicyJobSource();

    JobSourceDescriptor descriptor() const override;

    JobScoutReply *searchForJobs(const JobSearchProfile &searchProfile,
                                 QObject *replyParent) override;

private:
    QNetworkAccessManager networkAccessManager;
};
