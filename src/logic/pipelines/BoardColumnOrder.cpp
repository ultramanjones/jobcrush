#include "BoardColumnOrder.h"

#include <QCollator>

#include <algorithm>

namespace BoardColumnOrder {

namespace {

const QString dateFieldName = QStringLiteral("date");
const QString jobTitleFieldName = QStringLiteral("title");
const QString companyNameFieldName = QStringLiteral("company");

const QString ascendingText = QStringLiteral("ascending");
const QString descendingText = QStringLiteral("descending");

// The direction a sort starts in when its button is clicked for the first
// time. orderAfterClicking says why.
SortDirection firstDirectionFor(SortField sortField)
{
    return sortField == SortField::Date ? SortDirection::Descending
                                        : SortDirection::Ascending;
}

} // namespace

QStringList sortFieldNames()
{
    return { dateFieldName, jobTitleFieldName, companyNameFieldName };
}

bool sortFieldFromName(const QString &sortFieldName, SortField &sortField)
{
    if (sortFieldName == dateFieldName) {
        sortField = SortField::Date;
        return true;
    }
    if (sortFieldName == jobTitleFieldName) {
        sortField = SortField::JobTitle;
        return true;
    }
    if (sortFieldName == companyNameFieldName) {
        sortField = SortField::CompanyName;
        return true;
    }
    return false;
}

QString nameForSortField(SortField sortField)
{
    switch (sortField) {
    case SortField::Date:        return dateFieldName;
    case SortField::JobTitle:    return jobTitleFieldName;
    case SortField::CompanyName: return companyNameFieldName;
    }
    return dateFieldName; // never reached; stops a "no return value" warning
}

QString storageTextFor(const ColumnOrder &columnOrder)
{
    return nameForSortField(columnOrder.sortField) + QLatin1Char(' ')
           + (columnOrder.sortDirection == SortDirection::Ascending ? ascendingText
                                                                    : descendingText);
}

ColumnOrder columnOrderFromStorageText(const QString &storageText)
{
    // The text is two words: the field name, then the direction. If either
    // word is wrong, then the whole text is ignored and the default is used.
    // A setting that was saved wrong should never stop the board from drawing.
    ColumnOrder columnOrder;
    const QStringList words = storageText.simplified().split(QLatin1Char(' '));
    if (words.count() != 2) {
        return ColumnOrder();
    }
    if (!sortFieldFromName(words.at(0), columnOrder.sortField)) {
        return ColumnOrder();
    }
    if (words.at(1) == ascendingText) {
        columnOrder.sortDirection = SortDirection::Ascending;
    } else if (words.at(1) == descendingText) {
        columnOrder.sortDirection = SortDirection::Descending;
    } else {
        return ColumnOrder();
    }
    return columnOrder;
}

ColumnOrder orderAfterClicking(const ColumnOrder &currentOrder, SortField clickedField)
{
    ColumnOrder newOrder;
    newOrder.sortField = clickedField;
    if (clickedField == currentOrder.sortField) {
        newOrder.sortDirection = currentOrder.sortDirection == SortDirection::Ascending
            ? SortDirection::Descending
            : SortDirection::Ascending;
    } else {
        newOrder.sortDirection = firstDirectionFor(clickedField);
    }
    return newOrder;
}

QDateTime sortDateFor(const TargetedJob &targetedJob)
{
    if (targetedJob.campaign.pipelineStage == PipelineStage::Applied) {
        return targetedJob.campaign.appliedTimestamp;
    }
    return targetedJob.campaign.targetedTimestamp;
}

void sortCards(QList<TargetedJob> &cards, const ColumnOrder &columnOrder)
{
    const bool ascending = columnOrder.sortDirection == SortDirection::Ascending;

    if (columnOrder.sortField == SortField::Date) {
        // The steps for two cards, first and second:
        // 1. If neither card has a date, then neither goes first.
        // 2. If only one card has a date, then that card goes first. This
        //    keeps cards with no date at the bottom in both directions.
        // 3. If both have a date, then compare the dates. Ascending puts the
        //    older date first. Descending puts the newer date first.
        std::stable_sort(cards.begin(), cards.end(),
                         [ascending](const TargetedJob &first, const TargetedJob &second) {
            const QDateTime firstDate = sortDateFor(first);
            const QDateTime secondDate = sortDateFor(second);
            if (!firstDate.isValid() || !secondDate.isValid()) {
                return firstDate.isValid() && !secondDate.isValid();
            }
            return ascending ? firstDate < secondDate : secondDate < firstDate;
        });
        return;
    }

    // Names are compared the way a person reads a list. Capital and small
    // letters count as the same. Numbers inside a name are read as numbers,
    // so "Engineer 2" comes before "Engineer 10".
    QCollator nameCollator;
    nameCollator.setCaseSensitivity(Qt::CaseInsensitive);
    nameCollator.setNumericMode(true);

    const bool sortByJobTitle = columnOrder.sortField == SortField::JobTitle;
    std::stable_sort(cards.begin(), cards.end(),
                     [&nameCollator, ascending, sortByJobTitle](const TargetedJob &first,
                                                                const TargetedJob &second) {
        const QString &firstName = sortByJobTitle ? first.posting.positionTitle
                                                  : first.posting.companyName;
        const QString &secondName = sortByJobTitle ? second.posting.positionTitle
                                                   : second.posting.companyName;
        const int comparison = nameCollator.compare(firstName.trimmed(), secondName.trimmed());
        return ascending ? comparison < 0 : comparison > 0;
    });
}

} // namespace BoardColumnOrder
