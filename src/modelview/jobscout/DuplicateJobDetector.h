#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "../../model/JobPosting.h"
#include "AtsBoardDetector.h"
#include "UrlCanonicalizer.h"

// DuplicateJobDetector
//
// Decides whether two postings are the same job.
//
// This matters more than it sounds. The same job reaches Job Crush from
// several sources at once: an aggregator, the employer's own board, and a
// forwarded alert. Without this the board fills with the same job three times
// and the user has to work out which one to keep.
//
// Four tests, strongest first. The first one that applies decides:
//
//   1. Same hiring system, same employer account, same job id. This is proof.
//   2. Same URL once tracking is stripped. Also proof.
//   3. Same source and same id from that source. Proof within one source.
//   4. Same company and same title, and the locations do not contradict.
//      This one is a judgment call, so it is deliberately last and it is
//      strict: near enough is not good enough when the cost of being wrong is
//      hiding a real job the user never sees.
//
// No state, so it stays header-only.
class DuplicateJobDetector {
public:
    bool sameJob(const JobPosting &left, const JobPosting &right) const
    {
        // 1. The hiring system's own identity.
        const AtsBoardIdentity leftBoard = detector.identify(left.sourceUrl);
        const AtsBoardIdentity rightBoard = detector.identify(right.sourceUrl);
        if (leftBoard.namesOneJob() && rightBoard.namesOneJob()) {
            return leftBoard.asKey().compare(rightBoard.asKey(), Qt::CaseInsensitive) == 0;
        }

        // 2. The same link.
        if (!left.sourceUrl.isEmpty() && !right.sourceUrl.isEmpty()
                && canonicalizer.sameJob(left.sourceUrl, right.sourceUrl)) {
            return true;
        }

        // 3. The same id from the same source.
        if (!left.externalSourceId.isEmpty()
                && left.externalSourceId == right.externalSourceId
                && left.discoverySource == right.discoverySource) {
            return true;
        }

        // 4. Same company, same title, and the locations agree.
        const QString leftCompany = plainCompanyName(left.companyName);
        const QString rightCompany = plainCompanyName(right.companyName);
        if (leftCompany.isEmpty() || leftCompany != rightCompany) {
            return false;
        }
        if (plainTitle(left.positionTitle) != plainTitle(right.positionTitle)) {
            return false;
        }
        return locationsAgree(left, right);
    }

    // The first posting in a list that is the same job, or -1 when there is
    // none. Callers use this to merge rather than insert.
    int indexOfSameJob(const QList<JobPosting> &everyPosting,
                       const JobPosting &newPosting) const
    {
        for (int postingIndex = 0; postingIndex < everyPosting.count(); ++postingIndex) {
            if (sameJob(everyPosting.at(postingIndex), newPosting)) {
                return postingIndex;
            }
        }
        return -1;
    }

    // A company name with the legal suffix and punctuation removed, so
    // "Acme Robotics, Inc." and "Acme Robotics LLC" compare equal.
    static QString plainCompanyName(const QString &companyName)
    {
        QString plainName = companyName.toLower();

        static const QRegularExpression legalSuffixPattern(
            QStringLiteral("[,\\s]+(?:inc|inc\\.|llc|l\\.l\\.c\\.|ltd|ltd\\.|limited|corp"
                           "|corp\\.|corporation|co|co\\.|company|gmbh|s\\.a\\.|sa|bv|nv"
                           "|plc|pty|ag|ab|oy|as)\\.?\\s*$"),
            QRegularExpression::CaseInsensitiveOption);
        // Twice, because "Acme Robotics Co., Ltd." carries two of them.
        plainName.remove(legalSuffixPattern);
        plainName.remove(legalSuffixPattern);

        static const QRegularExpression punctuationPattern(
            QStringLiteral("[^a-z0-9 ]"));
        plainName.remove(punctuationPattern);
        return plainName.simplified();
    }

    // A title with the noise employers add for search engines removed, so
    // "Senior C++ Engineer (Remote)" and "Senior C++ Engineer - Remote"
    // compare equal. Seniority words are LEFT ALONE: a Senior Engineer and an
    // Engineer are two different jobs and merging them would hide one.
    static QString plainTitle(const QString &positionTitle)
    {
        QString plain = positionTitle.toLower();

        // A trailing tag in brackets or after a dash: "(remote)", "- hybrid".
        static const QRegularExpression trailingTagPattern(
            QStringLiteral("\\s*[\\(\\[\\-–—|/]\\s*"
                           "(?:remote|hybrid|on[\\s-]?site|contract|full[\\s-]?time"
                           "|part[\\s-]?time|w2|c2c|urgent|hiring now|new)"
                           "[^\\)\\]]*[\\)\\]]?\\s*$"),
            QRegularExpression::CaseInsensitiveOption);
        plain.remove(trailingTagPattern);

        // Anything after a pipe. A pipe in a job title is a tag the employer
        // added for search, never part of the job's name.
        static const QRegularExpression afterAPipePattern(QStringLiteral("\\s*\\|.*$"));
        plain.remove(afterAPipePattern);

        // A trailing place: "senior engineer - austin, tx". Only when it looks
        // like a place, because "engineer - backend" is a different job from
        // "engineer" and must not be flattened into it.
        static const QRegularExpression trailingPlacePattern(
            QStringLiteral("\\s*[\\-–—,]\\s*[a-z .]+,\\s*[a-z]{2,}\\s*$"),
            QRegularExpression::CaseInsensitiveOption);
        plain.remove(trailingPlacePattern);

        static const QRegularExpression punctuationPattern(
            QStringLiteral("[^a-z0-9+# ]"));
        plain.remove(punctuationPattern);
        return plain.simplified();
    }

    // Locations agree when they say the same thing, when one of them says
    // nothing, or when both are remote. They disagree when two different
    // cities are named, which is a real difference: the same title at the same
    // company in two cities is two jobs.
    static bool locationsAgree(const JobPosting &left, const JobPosting &right)
    {
        if (left.isRemoteRole && right.isRemoteRole) {
            return true;
        }
        const QString leftLocation = plainLocation(left.locationText);
        const QString rightLocation = plainLocation(right.locationText);
        if (leftLocation.isEmpty() || rightLocation.isEmpty()) {
            return true;
        }
        return leftLocation == rightLocation
            || leftLocation.contains(rightLocation)
            || rightLocation.contains(leftLocation);
    }

private:
    static QString plainLocation(const QString &locationText)
    {
        QString plain = locationText.toLower();
        static const QRegularExpression punctuationPattern(QStringLiteral("[^a-z0-9 ]"));
        plain.remove(punctuationPattern);
        // "remote" on its own carries no place, so it cannot disagree with one.
        plain.replace(QRegularExpression(QStringLiteral("\\bremote\\b")), QString());
        plain.replace(QRegularExpression(QStringLiteral("\\b(?:usa|us|united states)\\b")),
                      QString());
        return plain.simplified();
    }

    AtsBoardDetector detector;
    UrlCanonicalizer canonicalizer;
};
