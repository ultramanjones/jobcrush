#include <QTest>

#include "logic/pipelines/TypedDateReader.h"

// Tests for TypedDateReader, which reads the date a user types into the
// "Applied on" box on a board card.
//
// "Today" is fixed at 11 Oct 2026 so the answers never change.
class TestTypedDateReader : public QObject {
    Q_OBJECT

private slots:
    void readsWhatPeopleType_data()
    {
        QTest::addColumn<QString>("typedText");
        QTest::addColumn<QDate>("expectedDate");

        QTest::newRow("today")           << QStringLiteral("today")       << QDate(2026, 10, 11);
        QTest::newRow("Yesterday")       << QStringLiteral("Yesterday")   << QDate(2026, 10, 10);
        QTest::newRow("3 days ago")      << QStringLiteral("3 days ago")  << QDate(2026, 10, 8);
        QTest::newRow("1 day ago")       << QStringLiteral("1 day ago")   << QDate(2026, 10, 10);
        QTest::newRow("year first")      << QStringLiteral("2026-10-08")  << QDate(2026, 10, 8);
        QTest::newRow("US full")         << QStringLiteral("10/8/2026")   << QDate(2026, 10, 8);
        QTest::newRow("US no year")      << QStringLiteral("10/8")        << QDate(2026, 10, 8);
        QTest::newRow("US short year")   << QStringLiteral("10/8/26")     << QDate(2026, 10, 8);
        QTest::newRow("day month year")  << QStringLiteral("8 Oct 2026")  << QDate(2026, 10, 8);
        QTest::newRow("day month")       << QStringLiteral("8 oct")       << QDate(2026, 10, 8);
        QTest::newRow("month day, year") << QStringLiteral("Oct 8, 2026") << QDate(2026, 10, 8);
        QTest::newRow("full month, th")  << QStringLiteral("October 8th") << QDate(2026, 10, 8);
        QTest::newRow("month with dot")  << QStringLiteral("Oct. 8")      << QDate(2026, 10, 8);
        QTest::newRow("sept")            << QStringLiteral("sept 30")     << QDate(2026, 9, 30);
        QTest::newRow("no year, future means last year")
                                         << QStringLiteral("Dec 20")      << QDate(2025, 12, 20);
        QTest::newRow("numbers, future means last year")
                                         << QStringLiteral("12/20")       << QDate(2025, 12, 20);
    }

    void readsWhatPeopleType()
    {
        QFETCH(QString, typedText);
        QFETCH(QDate, expectedDate);

        const QLocale unitedStates(QLocale::English, QLocale::UnitedStates);
        QDate dateRead;
        QVERIFY2(TypedDateReader::readTypedDate(typedText, QDate(2026, 10, 11),
                                                unitedStates, dateRead),
                 qPrintable(typedText));
        QCOMPARE(dateRead, expectedDate);
    }

    void refusesWhatIsNotADate_data()
    {
        QTest::addColumn<QString>("typedText");
        QTest::newRow("empty")        << QString();
        QTest::newRow("word")         << QStringLiteral("banana");
        QTest::newRow("bad month")    << QStringLiteral("8 banana");
        QTest::newRow("no such day")  << QStringLiteral("2/30/2026");
        QTest::newRow("out of range") << QStringLiteral("13/40");
    }

    void refusesWhatIsNotADate()
    {
        QFETCH(QString, typedText);
        const QLocale unitedStates(QLocale::English, QLocale::UnitedStates);
        QDate dateRead;
        QVERIFY(!TypedDateReader::readTypedDate(typedText, QDate(2026, 10, 11),
                                                unitedStates, dateRead));
    }

    void readsDayFirstWhereTheLocaleDoes()
    {
        const QLocale unitedKingdom(QLocale::English, QLocale::UnitedKingdom);
        QDate dateRead;
        QVERIFY(TypedDateReader::readTypedDate(QStringLiteral("8/10/2026"), QDate(2026, 10, 11),
                                               unitedKingdom, dateRead));
        QCOMPARE(dateRead, QDate(2026, 10, 8));
    }

    void readsMonthFirstWhenNoLanguageIsSet()
    {
        QDate dateRead;
        QVERIFY(TypedDateReader::readTypedDate(QStringLiteral("10/8"), QDate(2026, 10, 11),
                                               QLocale::c(), dateRead));
        QCOMPARE(dateRead, QDate(2026, 10, 8));
    }

    void readsBackWhatItShows()
    {
        const QString shownText = TypedDateReader::editableDateTextFor(QDate(2026, 10, 8));
        QCOMPARE(shownText, QStringLiteral("8 Oct 2026"));

        QDate dateRead;
        QVERIFY(TypedDateReader::readTypedDate(shownText, QDate(2026, 10, 11),
                                               QLocale(QLocale::German, QLocale::Germany),
                                               dateRead));
        QCOMPARE(dateRead, QDate(2026, 10, 8));
    }
};

QTEST_GUILESS_MAIN(TestTypedDateReader)
#include "tst_TypedDateReader.moc"
