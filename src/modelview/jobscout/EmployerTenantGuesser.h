#pragma once

#include <QRegularExpression>
#include <QString>
#include <QStringList>

// EmployerTenantGuesser
//
// Guesses what a company calls itself on a hiring system, starting from the
// company's name.
//
// This is how Job Crush gets around LinkedIn. A forwarded LinkedIn alert says
// "Senior C++ Engineer at Acme Robotics" and gives a linkedin.com link Job
// Crush is not allowed to fetch. But most companies that post on LinkedIn also
// run their own board, and the account name on that board is almost always the
// company name with the spaces taken out. Guess it, ask the board, and the
// real posting comes back — from the employer, with the whole description, and
// with the job disappearing when it is filled.
//
// The guesses are cheap and wrong guesses cost one 404. A guess that is too
// loose is the expensive mistake: "acme" could be anyone's board, and matching
// a job to the wrong company is worse than not matching it at all. So the
// guesses stay tied to the whole name.
//
// No state, so it stays header-only.
class EmployerTenantGuesser {
public:
    // Board account names to try, most likely first. Empty when the company
    // name gives nothing to work with.
    static QStringList tenantGuessesFor(const QString &companyName)
    {
        const QString plainName = plainCompanyWords(companyName);
        if (plainName.isEmpty()) {
            return {};
        }

        const QStringList words = plainName.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (words.isEmpty()) {
            return {};
        }

        QStringList guesses;

        // "acmerobotics" — the common shape. Greenhouse board tokens look
        // like this almost every time.
        guesses.append(words.join(QString()));

        // "acme-robotics" — the other shape, used by some Lever and Ashby
        // boards. Only worth adding when the name is more than one word.
        if (words.count() > 1) {
            guesses.append(words.join(QLatin1Char('-')));
        }

        guesses.removeDuplicates();
        return guesses;
    }

private:
    // The company name reduced to lowercase words: no punctuation, no legal
    // suffix, no leading "the". Board accounts never carry any of that.
    static QString plainCompanyWords(const QString &companyName)
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

        static const QRegularExpression leadingArticlePattern(
            QStringLiteral("^the\\s+"), QRegularExpression::CaseInsensitiveOption);
        plainName.remove(leadingArticlePattern);

        // An ampersand is a word: "Johnson & Johnson" is "johnsonandjohnson"
        // on a board far more often than "johnsonjohnson".
        plainName.replace(QLatin1Char('&'), QStringLiteral(" and "));

        static const QRegularExpression punctuationPattern(QStringLiteral("[^a-z0-9 ]"));
        plainName.remove(punctuationPattern);

        return plainName.simplified();
    }
};
