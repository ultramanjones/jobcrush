#include <QtTest>

#include "logic/pipelines/BoardColumnOrder.h"

namespace {

// One card for the tests, with only the fields the sort reads.
TargetedJob cardFor(const QString &positionTitle, const QString &companyName,
                    PipelineStage pipelineStage, const QDateTime &targetedTimestamp,
                    const QDateTime &appliedTimestamp = QDateTime())
{
    TargetedJob card;
    card.posting.positionTitle = positionTitle;
    card.posting.companyName = companyName;
    card.campaign.pipelineStage = pipelineStage;
    card.campaign.targetedTimestamp = targetedTimestamp;
    card.campaign.appliedTimestamp = appliedTimestamp;
    return card;
}

// The job titles of the cards, top to bottom.
QStringList titlesOf(const QList<TargetedJob> &cards)
{
    QStringList titles;
    for (const TargetedJob &card : cards) {
        titles.append(card.posting.positionTitle);
    }
    return titles;
}

QDateTime dayInNovember(int dayOfMonth)
{
    return QDateTime(QDate(2026, 11, dayOfMonth), QTime(10, 0));
}

} // namespace

// Tests for BoardColumnOrder: the order of the cards in one board column.
class BoardColumnOrderTest : public QObject {
    Q_OBJECT

private slots:
    void savedTextRoundTrips()
    {
        BoardColumnOrder::ColumnOrder order;
        order.sortField = BoardColumnOrder::SortField::CompanyName;
        order.sortDirection = BoardColumnOrder::SortDirection::Descending;
        const QString savedText = BoardColumnOrder::storageTextFor(order);
        QCOMPARE(savedText, QStringLiteral("company descending"));

        const BoardColumnOrder::ColumnOrder readBack =
            BoardColumnOrder::columnOrderFromStorageText(savedText);
        QCOMPARE(readBack.sortField, BoardColumnOrder::SortField::CompanyName);
        QCOMPARE(readBack.sortDirection, BoardColumnOrder::SortDirection::Descending);
    }

    void badSavedTextGivesTheDefault()
    {
        for (const QString &badText : { QString(), QStringLiteral("banana"),
                                        QStringLiteral("date sideways"),
                                        QStringLiteral("date ascending extra") }) {
            const BoardColumnOrder::ColumnOrder order =
                BoardColumnOrder::columnOrderFromStorageText(badText);
            QCOMPARE(order.sortField, BoardColumnOrder::SortField::Date);
            QCOMPARE(order.sortDirection, BoardColumnOrder::SortDirection::Ascending);
        }
    }

    void clickingTheSameButtonFlipsAndANewButtonStartsFresh()
    {
        using BoardColumnOrder::SortDirection;
        using BoardColumnOrder::SortField;

        BoardColumnOrder::ColumnOrder order;
        order = BoardColumnOrder::orderAfterClicking(order, SortField::JobTitle);
        QCOMPARE(order.sortField, SortField::JobTitle);
        QCOMPARE(order.sortDirection, SortDirection::Ascending);

        order = BoardColumnOrder::orderAfterClicking(order, SortField::JobTitle);
        QCOMPARE(order.sortDirection, SortDirection::Descending);

        // The date starts newest first.
        order = BoardColumnOrder::orderAfterClicking(order, SortField::Date);
        QCOMPARE(order.sortField, SortField::Date);
        QCOMPARE(order.sortDirection, SortDirection::Descending);

        order = BoardColumnOrder::orderAfterClicking(order, SortField::Date);
        QCOMPARE(order.sortDirection, SortDirection::Ascending);
    }

    void sortsByTheDateTheJobWentOnTheBoard()
    {
        QList<TargetedJob> cards = {
            cardFor(QStringLiteral("Middle"), QStringLiteral("B"), PipelineStage::Saved,
                    dayInNovember(5)),
            cardFor(QStringLiteral("Newest"), QStringLiteral("C"), PipelineStage::Saved,
                    dayInNovember(9)),
            cardFor(QStringLiteral("Oldest"), QStringLiteral("A"), PipelineStage::Saved,
                    dayInNovember(1)),
        };
        BoardColumnOrder::ColumnOrder order;
        order.sortDirection = BoardColumnOrder::SortDirection::Descending;
        BoardColumnOrder::sortCards(cards, order);
        QCOMPARE(titlesOf(cards),
                 QStringList({ QStringLiteral("Newest"), QStringLiteral("Middle"),
                               QStringLiteral("Oldest") }));

        order.sortDirection = BoardColumnOrder::SortDirection::Ascending;
        BoardColumnOrder::sortCards(cards, order);
        QCOMPARE(titlesOf(cards),
                 QStringList({ QStringLiteral("Oldest"), QStringLiteral("Middle"),
                               QStringLiteral("Newest") }));
    }

    void theAppliedColumnSortsByTheAppliedDate()
    {
        // Put on the board in one order, applied in the other.
        QList<TargetedJob> cards = {
            cardFor(QStringLiteral("Applied last"), QStringLiteral("A"),
                    PipelineStage::Applied, dayInNovember(1), dayInNovember(15)),
            cardFor(QStringLiteral("Applied first"), QStringLiteral("B"),
                    PipelineStage::Applied, dayInNovember(10), dayInNovember(11)),
            cardFor(QStringLiteral("No date"), QStringLiteral("C"),
                    PipelineStage::Applied, dayInNovember(20)),
        };
        BoardColumnOrder::ColumnOrder order;
        order.sortDirection = BoardColumnOrder::SortDirection::Descending;
        BoardColumnOrder::sortCards(cards, order);
        QCOMPARE(titlesOf(cards),
                 QStringList({ QStringLiteral("Applied last"), QStringLiteral("Applied first"),
                               QStringLiteral("No date") }));

        // A card with no applied date stays at the bottom both ways.
        order.sortDirection = BoardColumnOrder::SortDirection::Ascending;
        BoardColumnOrder::sortCards(cards, order);
        QCOMPARE(titlesOf(cards),
                 QStringList({ QStringLiteral("Applied first"), QStringLiteral("Applied last"),
                               QStringLiteral("No date") }));
    }

    void sortsByCompanyAndTitleIgnoringCapitals()
    {
        QList<TargetedJob> cards = {
            cardFor(QStringLiteral("zeta engineer"), QStringLiteral("bravo"),
                    PipelineStage::Saved, dayInNovember(1)),
            cardFor(QStringLiteral("Alpha Engineer"), QStringLiteral("Charlie"),
                    PipelineStage::Saved, dayInNovember(2)),
            cardFor(QStringLiteral("Mid Engineer"), QStringLiteral("alpha"),
                    PipelineStage::Saved, dayInNovember(3)),
        };
        BoardColumnOrder::ColumnOrder order;
        order.sortField = BoardColumnOrder::SortField::CompanyName;
        BoardColumnOrder::sortCards(cards, order);
        QCOMPARE(titlesOf(cards),
                 QStringList({ QStringLiteral("Mid Engineer"), QStringLiteral("zeta engineer"),
                               QStringLiteral("Alpha Engineer") }));

        order.sortField = BoardColumnOrder::SortField::JobTitle;
        order.sortDirection = BoardColumnOrder::SortDirection::Descending;
        BoardColumnOrder::sortCards(cards, order);
        QCOMPARE(titlesOf(cards),
                 QStringList({ QStringLiteral("zeta engineer"), QStringLiteral("Mid Engineer"),
                               QStringLiteral("Alpha Engineer") }));
    }
};

QTEST_GUILESS_MAIN(BoardColumnOrderTest)
#include "tst_BoardColumnOrder.moc"
