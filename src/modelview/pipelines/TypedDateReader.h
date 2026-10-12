#pragma once

#include <QDate>
#include <QLocale>
#include <QString>

// TypedDateReader
//
// Turns a date a person typed into a QDate.
//
// The applied date on a board card can be typed by hand, because people do
// not always drag the card on the day they apply. They type what is in their
// head, so this accepts the common ways of writing a date:
//
//   today, yesterday, 3 days ago
//   2026-10-08
//   10/8/2026, 10/8        (or 8/10/2026, 8/10 where the locale puts the day first)
//   8 Oct 2026, 8 Oct
//   Oct 8 2026, Oct 8, 2026, October 8
//
// A date with no year means this year. If that lands in the future, it means
// last year, because nobody applies to a job next month.
//
// It only reads. It does not decide whether a date is allowed. That is the
// caller's job.
namespace TypedDateReader {

// Reads typedText. Returns true and fills dateRead when the text is a date it
// understands. Returns false and leaves dateRead alone when it is not.
//
// today is passed in, not looked up, so the reader gives the same answer every
// time it is tested.
bool readTypedDate(const QString &typedText,
                   const QDate &today,
                   const QLocale &locale,
                   QDate &dateRead);

// How the board shows a date the user may edit: "8 Oct 2026". The day, the
// month as a word, and the year. No reader anywhere mistakes it for a
// different day, and readTypedDate reads it back.
QString editableDateTextFor(const QDate &date);

} // namespace TypedDateReader
