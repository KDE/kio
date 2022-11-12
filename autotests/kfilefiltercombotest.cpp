/*
    This file is part of the KDE Frameworks
    SPDX-FileCopyrightText: 2022 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>

#include "kfilefiltercombo.h"

class KFileFilterComboTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testSetFilter();
    void testSetFilter_data();
    void testDefaultFilter();
#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    void testSetMimeFilter();
    void testSetMimeFilter_data();
    void testIsMimeFilter();
#endif
    void testSetMimeFilterDefault();
    void testSetMimeFilterDefault_data();
#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    void testFilters();
    void testFilters_data();
    void testFiltersMime();
    void testFiltersMime_data();
#endif
    void testShowsAllFiles();
    void testShowsAllFiles_data();
    void testCurrentFilter();
};

void KFileFilterComboTest::initTestCase()
{
    QLocale::setDefault(QLocale::c());
    qputenv("LC_ALL", "en_US.UTF-8");
    qputenv("LANG", "en_US.UTF-8");
    qputenv("LANGUAGE", "en");
}

void KFileFilterComboTest::testSetFilter()
{
    QFETCH(QString, filterString);
    QFETCH(QStringList, expectedComboboxText);

    KFileFilterCombo combo;
    combo.setFileFilters(KFileFilter::fromFilterString(filterString), KFileFilter{});

    QCOMPARE(combo.count(), expectedComboboxText.count());

    for (int i = 0; i < combo.count(); ++i) {
        const QString text = combo.itemData(i, Qt::DisplayRole).toString();
        QCOMPARE(text, expectedComboboxText[i]);
    }
}

void KFileFilterComboTest::testSetFilter_data()
{
    QTest::addColumn<QString>("filterString");
    QTest::addColumn<QStringList>("expectedComboboxText");

    QTest::addRow("extension_name") << "*.cpp|Sources (*.cpp)" << QStringList{"Sources (*.cpp)"};
    QTest::addRow("multiple_filter") << "*.cpp|Sources (*.cpp)\n*.h|Header files" << QStringList{"Sources (*.cpp)", "Header files"};
    QTest::addRow("mutiple_extension_multiple_filter")
        << "*.cpp *.cc *.C|C++ Source Files\n*.h *.H|Header files" << QStringList{"C++ Source Files", "Header files"};
    QTest::addRow("pattern_only") << "*.cpp" << QStringList{"*.cpp"};
}

void KFileFilterComboTest::testDefaultFilter()
{
    KFileFilterCombo combo;
#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    combo.setDefaultFilter("*.cpp|Sources (*.cpp)");
#else
    combo.setDefaultFileFilter(KFileFilter::fromFilterString("*.cpp|Sources (*.cpp)").first());
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    combo.setFilter(QString());
#else
    combo.setFileFilters({}, KFileFilter());
#endif
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "Sources (*.cpp)");

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    combo.setFilter("*.png|PNG Image (*.png)");
#else
    combo.setFileFilters(KFileFilter::fromFilterString("*.png|PNG Image (*.png)"), KFileFilter());
#endif
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "PNG Image (*.png)");

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    combo.setFilter(QString());
#else
    combo.setFileFilters({}, KFileFilter());
#endif
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "Sources (*.cpp)");
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterComboTest::testSetMimeFilter_data()
{
    QTest::addColumn<QStringList>("mimeTypes");
    QTest::addColumn<QString>("defaultType");
    QTest::addColumn<QStringList>("expectedComboboxText");

    QTest::addRow("one_mime") << QStringList{"image/png"} << "" << QStringList{"PNG image"};
    QTest::addRow("two_mime") << QStringList{"image/png", "image/jpeg"} << "" << QStringList{"PNG image, JPEG image", "PNG image", "JPEG image"};
    QTest::addRow("two_mime_with_default") << QStringList{"image/png", "image/jpeg"} << "image/jpeg" << QStringList{"PNG image", "JPEG image"};
    QTest::addRow("three_mime") << QStringList{"image/png", "image/jpeg", "text/plain"} << ""
                                << QStringList{"PNG image, JPEG image, plain text document", "PNG image", "JPEG image", "plain text document"};
    QTest::addRow("four_mime") << QStringList{"image/png", "image/jpeg", "text/plain", "application/mbox"} << ""
                               << QStringList{"All Supported Files", "PNG image", "JPEG image", "plain text document", "mailbox file"};
    QTest::addRow("duplicate_mime_comment") << QStringList{"application/metalink+xml", "application/metalink4+xml"} << ""
                                            << QStringList{"Metalink file (metalink), Metalink file (meta4)",
                                                           "Metalink file (metalink)",
                                                           "Metalink file (meta4)"};
    QTest::addRow("all") << QStringList{"application/octet-stream"} << "" << QStringList{"All Files"};
    QTest::addRow("all2") << QStringList{"application/octet-stream", "image/png"} << "" << QStringList{"PNG image", "All Files"};
    QTest::addRow("all_with_all_supported")
        << QStringList{"application/octet-stream", "image/png", "image/jpeg", "text/plain", "application/mbox"} << ""
        << QStringList{"All Supported Files", "PNG image", "JPEG image", "plain text document", "mailbox file", "All Files"};
    QTest::addRow("four_mime_with_default") << QStringList{"image/png", "image/jpeg", "text/plain", "application/mbox"} << "text/plain"
                                            << QStringList{"PNG image", "JPEG image", "plain text document", "mailbox file"};
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterComboTest::testSetMimeFilter()
{
    QFETCH(QStringList, mimeTypes);
    QFETCH(QString, defaultType);
    QFETCH(QStringList, expectedComboboxText);

    KFileFilterCombo combo;

    combo.setMimeFilter(mimeTypes, defaultType);

    QStringList actual;

    for (int i = 0; i < combo.count(); ++i) {
        const QString text = combo.itemData(i, Qt::DisplayRole).toString();
        actual << text;
    }

    QCOMPARE(actual, expectedComboboxText);
}
#endif

void KFileFilterComboTest::testSetMimeFilterDefault_data()
{
    QTest::addColumn<QStringList>("mimeTypes");
    QTest::addColumn<QString>("defaultType");
    QTest::addColumn<QString>("expectedComboboxCurrentText");

    QTest::addRow("one") << QStringList{"image/png"} << ""
                         << "PNG image";
    QTest::addRow("two") << QStringList{"image/png", "image/jpeg"} << ""
                         << "PNG image, JPEG image";
    QTest::addRow("two_with_default") << QStringList{"image/png", "image/jpeg"} << "image/png"
                                      << "PNG image";
    QTest::addRow("four_with_default") << QStringList{"image/png", "image/jpeg", "text/plain", "application/mbox"} << "text/plain"
                                       << "plain text document";
}

void KFileFilterComboTest::testSetMimeFilterDefault()
{
    QFETCH(QStringList, mimeTypes);
    QFETCH(QString, defaultType);
    QFETCH(QString, expectedComboboxCurrentText);

    KFileFilterCombo combo;

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    combo.setMimeFilter(mimeTypes, defaultType);
#else
    QVector<KFileFilter> filters;

    for (const QString &mimeType : mimeTypes) {
        filters << KFileFilter::fromMimeType(mimeType);
    }

    combo.setFileFilters(filters, KFileFilter::fromMimeType(defaultType));
#endif

    QCOMPARE(combo.currentText(), expectedComboboxCurrentText);
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterComboTest::testIsMimeFilter()
{
    KFileFilterCombo combo;

    combo.setFilter("*.png");
    QVERIFY(!combo.isMimeFilter());

    combo.setMimeFilter(QStringList{"image/jpeg"}, QString());
    QVERIFY(combo.isMimeFilter());
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterComboTest::testFilters_data()
{
    QTest::addColumn<QString>("filterString");
    QTest::addColumn<QStringList>("expectedFilters");

    QTest::addRow("extension_name") << "*.cpp|Sources (*.cpp)" << QStringList{"*.cpp|Sources (*.cpp)"};
    QTest::addRow("multiple_filter") << "*.cpp|Sources (*.cpp)\n*.h|Header files" << QStringList{"*.cpp|Sources (*.cpp)", "*.h|Header files"};
    QTest::addRow("mutiple_extension_multiple_filter")
        << "*.cpp *.cc *.C|C++ Source Files\n*.h *.H|Header files" << QStringList{"*.cpp *.cc *.C|C++ Source Files", "*.h *.H|Header files"};
    QTest::addRow("pattern_only") << "*.cpp" << QStringList{"*.cpp"};
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterComboTest::testFilters()
{
    QFETCH(QString, filterString);
    QFETCH(QStringList, expectedFilters);

    KFileFilterCombo combo;

    combo.setFilter(filterString);

    QCOMPARE(combo.filters(), expectedFilters);
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterComboTest::testFiltersMime_data()
{
    QTest::addColumn<QStringList>("mimeTypes");
    QTest::addColumn<QStringList>("expectedFilters");

    QTest::addRow("one") << QStringList{"image/png"} << QStringList{"image/png"};
    QTest::addRow("two") << QStringList{"image/png", "image/jpeg"} << QStringList{"image/png image/jpeg", "image/png", "image/jpeg"};
    QTest::addRow("four") << QStringList{"image/png", "image/jpeg", "text/calendar", "application/gzip"}
                          << QStringList{"image/png image/jpeg text/calendar application/gzip", "image/png", "image/jpeg", "text/calendar", "application/gzip"};
}
#endif

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
void KFileFilterComboTest::testFiltersMime()
{
    QFETCH(QStringList, mimeTypes);
    QFETCH(QStringList, expectedFilters);

    KFileFilterCombo combo;

    combo.setMimeFilter(mimeTypes, QString());

    QCOMPARE(combo.filters(), expectedFilters);
}
#endif

void KFileFilterComboTest::testShowsAllFiles_data()
{
    QTest::addColumn<QStringList>("mimeTypes");
    QTest::addColumn<QString>("defaultType");
    QTest::addColumn<bool>("expectedShowsAllFiles");

    QTest::addRow("one") << QStringList{"image/png"} << "" << false;
    QTest::addRow("two") << QStringList{"image/png", "text/plain"} << "" << true;
    QTest::addRow("two_with_default") << QStringList{"image/png", "text/plain"} << "text/plain" << false;
    QTest::addRow("three") << QStringList{"image/png", "text/plain", "text/calendar"} << "" << true;
    QTest::addRow("four") << QStringList{"image/png", "text/plain", "text/calendar", "image/jpeg"} << "" << true;
    QTest::addRow("four") << QStringList{"image/png", "text/plain", "text/calendar", "image/jpeg"} << "text/calendar" << false;
}

void KFileFilterComboTest::testShowsAllFiles()
{
    QFETCH(QStringList, mimeTypes);
    QFETCH(QString, defaultType);
    QFETCH(bool, expectedShowsAllFiles);

    KFileFilterCombo combo;

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    combo.setMimeFilter(mimeTypes, defaultType);
#else
    QVector<KFileFilter> filters;

    for (const QString &mimeType : mimeTypes) {
        filters << KFileFilter::fromMimeType(mimeType);
    }

    combo.setFileFilters(filters, KFileFilter::fromMimeType(defaultType));
#endif

    QCOMPARE(combo.showsAllTypes(), expectedShowsAllFiles);
}

void KFileFilterComboTest::testCurrentFilter()
{
    KFileFilterCombo combo;

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 101)
    combo.setFilter(
        QStringLiteral("*.xml *.a|Word 2003 XML (.xml)\n"
                       "*.odt|ODF Text Document (.odt)\n"
                       "*.xml *.b|DocBook (.xml)\n"
                       "*|Raw (*)"));

    // default filter is selected
    QCOMPARE(combo.currentFilter(), QStringLiteral("*.xml *.a"));

    // select 2nd duplicate XML filter (see bug 407642)
    combo.setCurrentFilter("*.xml *.b|DocBook (.xml)");
    QCOMPARE(combo.currentFilter(), QStringLiteral("*.xml *.b"));

    // select Raw '*' filter
    combo.setCurrentFilter("*|Raw (*)");
    QCOMPARE(combo.currentFilter(), QStringLiteral("*"));

    // select 2nd XML filter
    combo.setCurrentFilter("*.xml *.b|DocBook (.xml)");
    QCOMPARE(combo.currentFilter(), QStringLiteral("*.xml *.b"));
#else

    const KFileFilter wordFilter = KFileFilter::fromFilterString("*.xml *.a|Word 2003 XML (.xml)").first();
    const KFileFilter odtFilter = KFileFilter::fromFilterString("*.odt|ODF Text Document (.odt)").first();
    const KFileFilter docBookFilter = KFileFilter::fromFilterString("*.xml *.b|DocBook (.xml)").first();
    const KFileFilter rawFilter = KFileFilter::fromFilterString("*|Raw (*)").first();

    combo.setFileFilters({wordFilter, odtFilter, docBookFilter, rawFilter}, KFileFilter());

    // default filter is selected
    QCOMPARE(combo.currentFileFilter(), wordFilter);

    // select 2nd duplicate XML filter (see bug 407642)
    combo.setCurrentFileFilter(docBookFilter);
    QCOMPARE(combo.currentFileFilter(), docBookFilter);

    // select Raw '*' filter
    combo.setCurrentFileFilter(rawFilter);
    QCOMPARE(combo.currentFileFilter(), rawFilter);

    // select 2nd XML filter
    combo.setCurrentFileFilter(docBookFilter);
    QCOMPARE(combo.currentFileFilter(), docBookFilter);
#endif
}

QTEST_MAIN(KFileFilterComboTest)
#include "kfilefiltercombotest.moc"
