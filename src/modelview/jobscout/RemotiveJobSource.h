#pragma once

#include <QNetworkAccessManager>

#include "JobSourceProvider.h"

// RemotiveJobSource
//
// Remotive's public job API: remote roles, heavy on software and tech, no key
// of any kind. That makes it the source a brand-new install can find jobs
// with before the user has configured a single thing.
//
// Remotive accepts a search term, so the profile's first target title becomes
// the query and the rest of the ranking happens locally.
class RemotiveJobSource : public JobSourceProvider {
public:
    RemotiveJobSource();

    JobSourceDescriptor descriptor() const override;

    JobScoutReply *searchForJobs(const JobSearchProfile &searchProfile,
                                 QObject *replyParent) override;

private:
    QNetworkAccessManager networkAccessManager;
};
