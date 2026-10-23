#include "TypedDateReader.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

// Finds the month number for a typed month word. English month names always
// work, short or long ("oct", "october"). The locale's own names work too, so
// a German user can type "Okt". Returns 0 when the word is not a month.
int monthNumberForWord(const QString &typedWord, const QLocale &locale)
{
    const QString word = typedWord.toLower();
    if (word.length() < 3) {
        return 0;
    }

    static const QStringList englishMonthNames = {
        QStringLiteral("january"), QStringLiteral("february"), QStringLiteral("march"),
        QStringLiteral("april"),   QStringLiteral("may"),      QStringLiteral("june"),
        QStringLiteral("july"),    QStringLiteral("august"),   QStringLiteral("september"),
        QStringLiteral("october"), QStringLiteral("november"), QStringLiteral("december"),
    };
    for (int monthIndex = 0; monthIndex < englishMonthNames.size(); ++monthIndex) {
        // "sept" is a common way to shorten September, so any start of the
        // full name counts, as long as it is at least three letters.
        if (englishMonthNames.at(monthIndex).startsWith(word)) {
            return monthIndex + 1;
        }
    }

    for (int monthNumber = 1; monthNumber <= 12; ++monthNumber) {
        const QString longName = locale.monthName(monthNumber, QLocale::LongFormat).toLower();
        const QString shortName = locale.monthName(monthNumber, QLocale::ShortFormat)
                                      .toLower().remove(QLatin1Char('.'));
        if (word == longName || word == shortName) {
            return monthNumber;
        }
    }
    return 0;
}

// True when the locale writes the day before the month in a short date, like
// 8/10/2026 for the 8th of October. The US writes the month first.
bool localeWritesDayFirst(const QLocale &locale)
{
    // A machine with no language set reports the plain C locale, whose short
    // date happens to put the day first. That says nothing about the person,
    // so it falls back to month first, the way Job Crush's own hints are written.
    if (locale.language() == QLocale::C) {
        return false;
    }
    const QString shortFormat = locale.dateFormat(QLocale::ShortFormat);
    const qsizetype dayPosition = shortFormat.indexOf(QLatin1Char('d'));
    const qsizetype monthPosition = shortFormat.indexOf(QLatin1Char('M'));
    return dayPosition >= 0 && monthPosition >= 0 && dayPosition < monthPosition;
}

// Builds the date. When the user left the year off, it is this year, unless
// that puts it in the future, in which case it is last year.
bool buildDate(int year, int month, int day, bool yearWasTyped,
               const QDate &today, QDate &dateRead)
{
    if (!yearWasTyped) {
        year = today.year();
    } else if (year < 100) {
        year += 2000; // "10/8/26"
    }

    QDate builtDate(year, month, day);
    if (!builtDate.isValid()) {
        return false;
    }
    if (!yearWasTyped && builtDate > today) {
        builtDate = QDate(year - 1, month, day);
        if (!builtDate.isValid()) {
            return false; // 29 Feb, last year was not a leap year
        }
    }
    dateRead = builtDate;
    return true;
}

} // namespace

namespace TypedDateReader {

bool readTypedDate(const QString &typedText,
                   const QDate &today,
                   const QLocale &locale,
                   QDate &dateRead)
{
    // Lower case, commas gone, "Oct." read as "Oct", and runs of spaces
    // squeezed to one, so "Oct 8,  2026" and "oct 8 2026" are the same text.
    QString text = typedText.trimmed().toLower();
    text.remove(QLatin1Char(','));
    text.replace(QRegularExpression(QStringLiteral("([a-z])\\.")), QStringLiteral("\\1"));
    text = text.simplified();
    if (text.isEmpty()) {
        return false;
    }

    if (text == QStringLiteral("today")) {
        dateRead = today;
        return true;
    }
    if (text == QStringLiteral("yesterday")) {
        dateRead = today.addDays(-1);
        return true;
    }

    // "3 days ago"
    static const QRegularExpression daysAgoPattern(
        QStringLiteral("^(\\d{1,3}) days? ago$"));
    QRegularExpressionMatch match = daysAgoPattern.match(text);
    if (match.hasMatch()) {
        dateRead = today.addDays(-match.captured(1).toInt());
        return true;
    }

    // "2026-10-08". Year first is never ambiguous.
    static const QRegularExpression yearFirstPattern(
        QStringLiteral("^(\\d{4})-(\\d{1,2})-(\\d{1,2})$"));
    match = yearFirstPattern.match(text);
    if (match.hasMatch()) {
        return buildDate(match.captured(1).toInt(), match.captured(2).toInt(),
                         match.captured(3).toInt(), true, today, dateRead);
    }

    // "10/8/2026", "10/8", "10-8-26". Which number is the day depends on the
    // locale, because that is how the person typing it was taught to write it.
    static const QRegularExpression numbersOnlyPattern(
        QStringLiteral("^(\\d{1,2})[/.-](\\d{1,2})(?:[/.-](\\d{2}|\\d{4}))?$"));
    match = numbersOnlyPattern.match(text);
    if (match.hasMatch()) {
        const int firstNumber = match.captured(1).toInt();
        const int secondNumber = match.captured(2).toInt();
        const bool yearWasTyped = !match.captured(3).isEmpty();
        const int year = yearWasTyped ? match.captured(3).toInt() : 0;
        if (localeWritesDayFirst(locale)) {
            return buildDate(year, secondNumber, firstNumber, yearWasTyped, today, dateRead);
        }
        return buildDate(year, firstNumber, secondNumber, yearWasTyped, today, dateRead);
    }

    // "8 oct 2026", "8 october"
    static const QRegularExpression dayThenMonthPattern(
        QStringLiteral("^(\\d{1,2})(?:st|nd|rd|th)? ([^\\d\\s]+)(?: (\\d{4}))?$"));
    match = dayThenMonthPattern.match(text);
    if (match.hasMatch()) {
        const int monthNumber = monthNumberForWord(match.captured(2), locale);
        if (monthNumber == 0) {
            return false;
        }
        const bool yearWasTyped = !match.captured(3).isEmpty();
        return buildDate(yearWasTyped ? match.captured(3).toInt() : 0, monthNumber,
                         match.captured(1).toInt(), yearWasTyped, today, dateRead);
    }

    // "oct 8 2026", "october 8th"
    static const QRegularExpression monthThenDayPattern(
        QStringLiteral("^([^\\d\\s]+) (\\d{1,2})(?:st|nd|rd|th)?(?: (\\d{4}))?$"));
    match = monthThenDayPattern.match(text);
    if (match.hasMatch()) {
        const int monthNumber = monthNumberForWord(match.captured(1), locale);
        if (monthNumber == 0) {
            return false;
        }
        const bool yearWasTyped = !match.captured(3).isEmpty();
        return buildDate(yearWasTyped ? match.captured(3).toInt() : 0, monthNumber,
                         match.captured(2).toInt(), yearWasTyped, today, dateRead);
    }

    return false;
}

QString editableDateTextFor(const QDate &date)
{
    if (!date.isValid()) {
        return QString();
    }
    // Always English month words, so the text reads back the same way on any
    // machine. QLocale::c() gives "Oct", whatever the system language is.
    return QLocale::c().toString(date, QStringLiteral("d MMM yyyy"));
}

} // namespace TypedDateReader
