#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include "../../model/JobPosting.h"

class JobScoutReply;

// PostedJobPageReader
//
// Reads one job off the web page the user pasted.
//
// The link is the job's own page on the employer's site, or on a hiring
// system Job Crush has no reader for. Job Crush fetches that one page and
// looks for the job in it. Nothing is guessed and nothing else is fetched.
//
// Two ways to read the page, tried in order:
//
//   1. The structured block. Most job pages carry a hidden block of data in
//      a format Google defined, called JSON-LD, with a type of "JobPosting".
//      Google only lists a job in its Jobs search when the page has one, so
//      nearly every employer includes it. It names the title, the company,
//      the place, the description and the date, each in its own labeled
//      field. When it is there, the job is read exactly.
//
//   2. The page title. When the block is missing, the page's title tag and
//      its description line are the best that can be had. The reply says so,
//      so the user knows to check the details.
//
// The reply finishes with one posting when a job was read and none when the
// page had no job in it. It fails, with sourceHadTrouble set, when the page
// could not be fetched at all: the site turned Job Crush away, or the
// connection failed.
//
// Some sites send a page that is empty until a browser runs its scripts.
// Job Crush has no browser, so that page reads as "no job here". The message
// says what to do instead.
class PostedJobPageReader : public QObject {
    Q_OBJECT
public:
    explicit PostedJobPageReader(QObject *parent = nullptr);

    // Ownership: the reply is parented to replyParent. Callers should
    // deleteLater() it once finished or failed has fired.
    JobScoutReply *readJobFromPage(const QString &pageUrl, QObject *replyParent);

    // True when the last posting read came from the page title alone, so
    // the caller can warn the user to check the details. Reset on every
    // read.
    bool lastReadUsedThePageTitleOnly() const;

    // Reads a job out of page HTML. Public so it can be tested without a
    // network. Returns false when no job could be read.
    bool readJobFromHtml(const QString &pageHtml, const QString &pageUrl,
                         JobPosting &jobPosting, bool &usedThePageTitleOnly) const;

private:
    QNetworkAccessManager networkAccessManager;
    bool storedLastReadUsedThePageTitleOnly = false;
};
