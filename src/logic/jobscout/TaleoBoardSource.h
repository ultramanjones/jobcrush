#pragma once

#include <QDate>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QString>

#include <functional>
#include <memory>

#include "../../model/JobPosting.h"

#include "EmployerBoardProvider.h"

// TaleoBoardSource
//
// Reads an employer's Taleo careers site. Taleo is Oracle's older hiring
// system, still used by many large employers.
//
// Taleo has no documented public API. Its careers pages load their jobs from
// a JSON service on the same host, and Job Crush asks that service the same
// way the page does. Checked live on 2026-10-23, through a browser, against
// hdr.taleo.net (careers section "ex") and cu.taleo.net (careers section "2"):
//
//   Job link:   https://{account}.taleo.net/careersection/{section}/jobdetail.ftl?job={number}&lang=en
//   Search:     POST https://{account}.taleo.net/careersection/rest/jobboard/searchjobs?lang=en&portal={portal}
//               with a JSON body; see searchRequestBody.
//
// What the live answers showed:
//   1. The search needs a "portal" number. It is in the careers section's own
//      search page, jobsearch.ftl, as portalNo: '...' or a portal= link. Job
//      Crush reads it once per site and keeps it.
//   2. The search answer is {"requisitionList":[...],"pagingData":
//      {"currentPageNo","pageSize","totalCount"},"careerSectionUnAvailable"}.
//      25 jobs a page.
//   3. Each job has "contestNo" (the number in the job link) and "column", a
//      list of text. Which column holds what changes from site to site.
//      "linkedColumn" says which one is the title, and "locationsColumns"
//      which one holds the places, as a JSON list in a string. The posting
//      date, when a site shows one, is in another column, as text like
//      "Oct 23, 2026".
//   4. Searching for a job's number finds exactly that job, and a number that
//      does not exist finds nothing. That is how one job is checked.
//   5. The search answer has no description. The job's own page has it, in a
//      script line: api.fillList('requisitionDescriptionInterface',
//      'descRequisition', [...]). The values in that list line up with the
//      names in the "_hlid" list right above it. Long text starts with "!*!"
//      and is URL-encoded HTML.
//
// The tenant string Job Crush keeps for a Taleo employer is
// "{account}/{section}", for example "hdr/ex".
class TaleoBoardSource : public EmployerBoardProvider {
public:
    TaleoBoardSource();

    QString boardName() const override;

    JobScoutReply *fetchEveryJobForEmployer(const QString &tenant,
                                            QObject *replyParent) override;

    JobScoutReply *fetchJobsLikeTitle(const QString &tenant,
                                      const QString &positionTitle,
                                      QObject *replyParent) override;

    JobScoutReply *fetchOneJob(const AtsBoardIdentity &boardIdentity,
                               QObject *replyParent) override;

    // --- The reading, kept apart from the network so it can be tested ----

    // Splits "hdr/ex" into the account and the careers section. Returns false
    // when the string is not in that form.
    static bool readTenant(const QString &taleoTenant, QString &accountName,
                           QString &careerSectionName);

    // The portal number in a careers section's search page, or empty.
    static QString portalNumberFromSearchPage(const QString &searchPageHtml);

    // One job from a search answer.
    static JobPosting postingFromRequisition(const QJsonObject &requisitionObject,
                                             const QString &taleoTenant);

    // The job's description, as plain text, from its own page. Empty when
    // the page does not have the usual script line.
    static QString descriptionFromJobPage(const QString &jobPageHtml);

    // The posting date in a search answer's column, or an invalid date.
    static QDate postedDateFromColumnText(const QString &columnText);

    // How many pages of 25 a whole-site read takes before it stops.
    static constexpr int maxPagesPerSite = 40;

private:
    struct TaleoSearch;

    // Finds the site's portal number, then calls whenPortalIsKnown with it.
    // Fails the reply when the number cannot be found.
    void withPortalNumber(const QString &accountName, const QString &careerSectionName,
                          JobScoutReply *scoutReply,
                          const std::function<void(const QString &)> &whenPortalIsKnown);

    // Reads pages of a search until the site runs out, or pagesAllowed.
    JobScoutReply *searchSite(const QString &taleoTenant, const QString &searchWords,
                              int pagesAllowed, QObject *replyParent);
    void readSearchPage(const std::shared_ptr<TaleoSearch> &search);

    QNetworkAccessManager networkAccessManager;

    // Portal numbers already found, by "{account}/{section}".
    QHash<QString, QString> knownPortalNumbers;
};
