#pragma once

#include <QNetworkAccessManager>

#include "JobSourceProvider.h"

// ArbeitnowJobSource
//
// Arbeitnow's open job-board API: no key, and its listings come straight out
// of company hiring systems (Greenhouse, Lever, SmartRecruiters, Recruitee
// and friends) rather than from a board that rewrote them. It also flags visa
// sponsorship, which nothing else free does.
//
// It takes no search terms — it hands back its latest page. That is fine:
// JobScout ranks everything locally anyway, so a site with no search box is
// still worth having. Job Crush says so plainly rather than pretending the
// user's query was honored.
class ArbeitnowJobSource : public JobSourceProvider {
public:
    ArbeitnowJobSource();

    JobSourceDescriptor descriptor() const override;

    JobScoutReply *searchForJobs(const JobSearchProfile &searchProfile,
                                 QObject *replyParent) override;

private:
    QNetworkAccessManager networkAccessManager;
};
