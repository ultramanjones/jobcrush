#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>

#include "../../model/JobPosting.h"

#include "EmployerBoardProvider.h"

// SmartRecruitersBoardSource
//
// Reads an employer's SmartRecruiters job board. SmartRecruiters publishes
// every public posting through an official API with no key.
//
//   A page of jobs: GET https://api.smartrecruiters.com/v1/companies/{tenant}/postings?limit=100&offset=0
//   A search:       the same, with &q=words
//   One job:        GET https://api.smartrecruiters.com/v1/companies/{tenant}/postings/{id}
//
// Checked live on 2026-10-16 against BoschGroup, through a browser:
//   - The list answer is {"offset","limit","totalFound","content":[...]}.
//     Each item has id, name, company.name, releasedDate, and location with
//     fullLocation, remote and hybrid. It has NO description and NO public
//     link; only the one-job answer has those (jobAd.sections.*.text as HTML,
//     and postingUrl).
//   - A limit over 100 is quietly cut to 100. It is not an error.
//   - A company name SmartRecruiters does not know answers 200 with
//     totalFound 0, the same as a company with no jobs. There is no 404 to
//     tell the two apart.
//   - q= matches loosely: "software engineer" also returned maintenance and
//     sales jobs. The resolver's own title matching does the real work.
//   - Big employers are big: Bosch had 5,073 jobs. A whole-board read stops
//     after maxPagesPerBoard pages and says it stopped (see
//     JobScoutReply::setReadOnlyPartOfTheBoard), so jobs past the stop are
//     never marked gone.
//
// The tenant is the company part of a jobs.smartrecruiters.com link, which
// AtsBoardDetector already pulls out.
class SmartRecruitersBoardSource : public EmployerBoardProvider {
public:
    SmartRecruitersBoardSource();

    QString boardName() const override;

    JobScoutReply *fetchEveryJobForEmployer(const QString &tenant,
                                            QObject *replyParent) override;

    JobScoutReply *fetchJobsLikeTitle(const QString &tenant,
                                      const QString &positionTitle,
                                      QObject *replyParent) override;

    JobScoutReply *fetchOneJob(const AtsBoardIdentity &boardIdentity,
                               QObject *replyParent) override;

    // One job from either answer (a list item or the one-job answer) as a
    // JobPosting. Public so it can be tested against a real answer.
    static JobPosting postingFromJob(const QJsonObject &jobObject, const QString &tenant);

    // How many pages of 100 a whole-board read takes before it stops.
    static constexpr int maxPagesPerBoard = 10;

private:
    JobScoutReply *fetchPages(const QString &tenant, const QString &searchWords,
                              int pagesAllowed, QObject *replyParent);

    QNetworkAccessManager networkAccessManager;
};
