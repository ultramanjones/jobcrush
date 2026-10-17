#pragma once

#include <QRegularExpression>
#include <QString>

#include "../../model/JobPosting.h"

// ClearanceDetector
//
// Says whether a posting asks for a security clearance. Defense jobs often
// do, and many people cannot get one, so the row says so up front. It is a
// flag, never a filter: the job stays in the list, because plenty of people
// do hold a clearance and those jobs are worth more to them.
//
// Plain word matching on the title and the description. No state, so it
// stays header-only.
namespace ClearanceDetector {

inline bool asksForClearance(const JobPosting &jobPosting)
{
    static const QRegularExpression clearancePattern(
        QStringLiteral("\\b(security clearance|clearance (is )?required|active clearance|"
                       "secret clearance|top secret|ts/sci|ts sci|"
                       "(obtain|maintain|hold) (a |an )?(dod |us |u\\.s\\. )?"
                       "(security )?clearance|polygraph)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return clearancePattern.match(jobPosting.positionTitle).hasMatch()
           || clearancePattern.match(jobPosting.fullDescriptionText).hasMatch();
}

} // namespace ClearanceDetector
