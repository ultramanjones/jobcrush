#pragma once

#include <QDate>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QString>

#include "EmployerBoardProvider.h"
#include "../../model/JobPosting.h"

// WorkdayBoardSource
//
// Reads an employer's Workday careers site.
//
// Most large defense, aerospace and medical-device employers post their jobs
// on Workday, not on Greenhouse, Lever or Ashby. Without this, a pasted link
// from those employers finds nothing.
//
// Workday has no documented public API. Its careers pages load their jobs from
// a JSON service the browser calls, and that is what this reads. It can change
// without warning, so every failure says exactly what came back.
//
// Checked against a live site (ngc.wd1, Northrop Grumman) on 11 Oct 2026:
//
//   Search:   POST https://{tenant}.{shard}.myworkdayjobs.com/wday/cxs/{tenant}/{site}/jobs
//             body {"appliedFacets":{},"limit":20,"offset":0,"searchText":"..."}
//             -> total, jobPostings[] of title, externalPath, timeType,
//                locationsText, postedOn, bulletFields
//
//   One job:  GET  https://{tenant}.{shard}.myworkdayjobs.com/wday/cxs/{tenant}/{site}{externalPath}
//             -> jobPostingInfo: title, jobDescription (HTML), location,
//                additionalLocations, startDate (yyyy-MM-dd), timeType,
//                jobReqId, externalUrl, country.descriptor
//             -> 404 when the job is gone
//
// Traps found in the live answers:
//
//   1. A limit above 20 is refused with HTTP 400. Pages of 20, always.
//   2. hiringOrganization.name is NOT the company. It is an internal office
//      name, "0090 CORP-Corporate Office". The company name comes from the
//      lead the user gave, or from the site name in the link.
//   3. postedOn is display text, "Posted 14 Days Ago". The one-job answer has
//      startDate as a real date, so that is used instead when there is one.
//   4. Search ranks loosely. Searching an exact title puts other titles first,
//      so a title match has to be found by reading the results, not by taking
//      the top one.
//   5. During Workday's maintenance the site redirects to a page on
//      community.workday.com. That is trouble to report, not "no jobs".
//
// The tenant string Job Crush keeps for a Workday employer is
// "{tenant}.{shard}/{site}", for example "ngc.wd1/Northrop_Grumman_External_Site".
// Workday needs all three parts to find a job, so they travel together.
//
// The job id is the job's path on that site without the leading slash:
// "job/United-States-Georgia-Warner-Robins/Embedded---Principal-Software-Engineer_R10252946".
class WorkdayBoardSource : public EmployerBoardProvider {
public:
    WorkdayBoardSource();

    QString boardName() const override;

    // Pages through the whole site, 20 at a time, up to a cap. Big employers
    // list thousands of jobs, so a title search is the better way to find one.
    JobScoutReply *fetchEveryJobForEmployer(const QString &tenant,
                                            QObject *replyParent) override;

    // Asks Workday's own search for the title and returns the first few pages
    // of what it finds. Much cheaper than reading the whole site.
    JobScoutReply *fetchJobsLikeTitle(const QString &tenant,
                                      const QString &positionTitle,
                                      QObject *replyParent) override;

    JobScoutReply *fetchOneJob(const AtsBoardIdentity &boardIdentity,
                               QObject *replyParent) override;

    // --- The reading, kept apart from the network so it can be tested ----

    // The three parts of a Workday tenant string. Returns false when the
    // string is not in "{tenant}.{shard}/{site}" form.
    static bool splitTenant(const QString &workdayTenant,
                            QString &hostPrefix, QString &cxsTenant, QString &siteName);

    // "Northrop_Grumman_External_Site" -> "Northrop Grumman".
    // "L3HarrisCareers" -> "L3Harris". A stand-in for when nobody told Job
    // Crush the company's name.
    static QString companyNameFromSiteName(const QString &siteName);

    // "Posted Today", "Posted Yesterday", "Posted 3 Days Ago",
    // "Posted 30+ Days Ago". Returns an invalid date when the text is
    // something else.
    static QDate postedDateFromDisplayText(const QString &postedOnText, const QDate &today);

    // One row of a search answer. No description: the search does not send one.
    static JobPosting postingFromSearchEntry(const QJsonObject &searchEntry,
                                             const QString &workdayTenant);

    // The whole answer for one job.
    static JobPosting postingFromJobDetail(const QJsonObject &jobDetailAnswer,
                                           const QString &workdayTenant,
                                           const QString &jobPath);

private:
    QNetworkAccessManager networkAccessManager;

    // Shared by both search calls. searchText empty means every job.
    JobScoutReply *searchSite(const QString &workdayTenant,
                              const QString &searchText,
                              int mostPagesToRead,
                              QObject *replyParent);
};
