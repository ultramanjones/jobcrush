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
                && left.postingSource == right.postingSource) {
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
    //
    // The steps, in order:
    //  1. Lowercase everything.
    //  2. Remove one legal suffix from the END of the name. The pattern is:
    //     a run of commas or spaces, then one suffix word (inc, llc, ltd,
    //     corp, gmbh, plc and the rest, with or without dots), then an
    //     optional dot, then optional spaces, then the end of the text. A
    //     suffix in the middle of a name is never touched.
    //  3. Do step 2 once more, for names that carry two suffixes.
    //  4. Remove every character that is not a letter, a digit or a space.
    //  5. Trim the ends and squeeze runs of spaces down to one.
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
    //
    // The steps, in order:
    //  1. Lowercase everything.
    //  2. Remove a trailing tag. The pattern is: optional spaces, then one
    //     opener ( "(", "[", a dash, "|" or "/" ), then optional spaces,
    //     then one tag word (remote, hybrid, on-site, contract, full-time,
    //     part-time, w2, c2c, urgent, hiring now, new), then anything up to
    //     an optional closing bracket, then the end of the text.
    //  3. Remove a pipe and everything after it.
    //  4. Remove a trailing place, such as "- austin, tx". See the pattern
    //     below for what counts as a place.
    //  5. Remove every character that is not a letter, a digit, "+", "#" or
    //     a space. "+" and "#" stay so "c++" and "c#" are not flattened
    //     into "c".
    //  6. Trim the ends and squeeze runs of spaces down to one.
    static QString plainTitle(const QString &positionTitle)
    {
        QString plainTitleText = positionTitle.toLower();

        // A trailing tag in brackets or after a dash: "(remote)", "- hybrid".
        static const QRegularExpression trailingTagPattern(
            QStringLiteral("\\s*[\\(\\[\\-–—|/]\\s*"
                           "(?:remote|hybrid|on[\\s-]?site|contract|full[\\s-]?time"
                           "|part[\\s-]?time|w2|c2c|urgent|hiring now|new)"
                           "[^\\)\\]]*[\\)\\]]?\\s*$"),
            QRegularExpression::CaseInsensitiveOption);
        plainTitleText.remove(trailingTagPattern);

        // Anything after a pipe. A pipe in a job title is a tag the employer
        // added for search, never part of the job's name.
        static const QRegularExpression afterAPipePattern(QStringLiteral("\\s*\\|.*$"));
        plainTitleText.remove(afterAPipePattern);

        // A trailing place: "senior engineer - austin, tx". Only when it looks
        // like a place, because "engineer - backend" is a different job from
        // "engineer" and must not be flattened into it.
        //
        // The pattern is: optional spaces, then a dash or a comma, then
        // optional spaces, then letters, spaces and dots (the city), then a
        // comma, then optional spaces, then two or more letters (the state or
        // country), then optional spaces, then the end of the text.
        static const QRegularExpression trailingPlacePattern(
            QStringLiteral("\\s*[\\-–—,]\\s*[a-z .]+,\\s*[a-z]{2,}\\s*$"),
            QRegularExpression::CaseInsensitiveOption);
        plainTitleText.remove(trailingPlacePattern);

        static const QRegularExpression punctuationPattern(
            QStringLiteral("[^a-z0-9+# ]"));
        plainTitleText.remove(punctuationPattern);
        return plainTitleText.simplified();
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
        // One place written inside the other also agrees: "pittsburgh" and
        // "pittsburgh pa" are the same city.
        return leftLocation == rightLocation
            || leftLocation.contains(rightLocation)
            || rightLocation.contains(leftLocation);
    }

private:
    // A location cut down to the words that name a place. Lowercase, no
    // punctuation, and with "remote" and the country words "usa", "us" and
    // "united states" removed as whole words. Those words say nothing about
    // which city, so two postings must not disagree over them.
    static QString plainLocation(const QString &locationText)
    {
        QString plainLocationText = locationText.toLower();
        static const QRegularExpression punctuationPattern(QStringLiteral("[^a-z0-9 ]"));
        plainLocationText.remove(punctuationPattern);
        // "remote" on its own carries no place, so it cannot disagree with one.
        plainLocationText.replace(QRegularExpression(QStringLiteral("\\bremote\\b")), QString());
        plainLocationText.replace(
            QRegularExpression(QStringLiteral("\\b(?:usa|us|united states)\\b")), QString());
        return plainLocationText.simplified();
    }

    AtsBoardDetector detector;
    UrlCanonicalizer canonicalizer;
};
