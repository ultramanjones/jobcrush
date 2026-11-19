#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include "JobPipelines.h"

// BoardColumnOrder
//
// Decides the order of the cards inside one column of the board. Each column
// has its own order, and the user picks it with the sort buttons at the top
// of the column.
//
// A card can be sorted three ways:
//  - by date,
//  - by job title, A to Z,
//  - by company name, A to Z.
// Each way can be flipped: newest first or oldest first, A to Z or Z to A.
//
// The date is the date the job was put on the board, except in the Applied
// column. There it is the date the user applied, because that is the date
// that matters once a job is sent.
//
// This is plain logic with no storage. AppPreferences saves the user's
// choice for each column as the text that storageTextFor makes.
namespace BoardColumnOrder {

// What the cards are sorted by.
enum class SortField {
    Date,
    JobTitle,
    CompanyName,
};

// Which way the sort runs. Ascending puts the oldest date first, or A first.
// Descending puts the newest date first, or Z first.
enum class SortDirection {
    Ascending,
    Descending,
};

// One column's order: what it sorts by, and which way.
struct ColumnOrder {
    SortField sortField = SortField::Date;
    SortDirection sortDirection = SortDirection::Ascending;
};

// The names QML uses for the three sort buttons, left to right: "date",
// "title", "company".
QStringList sortFieldNames();

// Reads a sort button's name. Returns false for a name that is not one of
// the three, and leaves sortField as it was.
bool sortFieldFromName(const QString &sortFieldName, SortField &sortField);

// The name of a sort field: "date", "title" or "company".
QString nameForSortField(SortField sortField);

// The text a column's order is saved as: "date descending", "title ascending".
QString storageTextFor(const ColumnOrder &columnOrder);

// Reads saved text back into an order. Empty or unreadable text gives the
// order the board had before sorting existed: by date, oldest first.
ColumnOrder columnOrderFromStorageText(const QString &storageText);

// The order after the user clicks one sort button.
//
// If the button is the one already in use, then the direction flips.
// If it is a different button, then that sort starts in its first
// direction: newest first for the date, A to Z for the two names. Newest
// first is the first direction for the date because the newest jobs are the
// ones the user is most likely looking for.
ColumnOrder orderAfterClicking(const ColumnOrder &currentOrder, SortField clickedField);

// The date a card is sorted by. For a card in the Applied column this is the
// date the user applied. For every other column it is the date the job was
// put on the board. An empty date means the card has none.
QDateTime sortDateFor(const TargetedJob &targetedJob);

// Puts the cards of one column in the order given. Cards with no date go
// last, whichever way the date sort runs. Cards that tie are kept in the
// order the board stores them in, so the column does not shuffle on redraw.
void sortCards(QList<TargetedJob> &cards, const ColumnOrder &columnOrder);

} // namespace BoardColumnOrder
