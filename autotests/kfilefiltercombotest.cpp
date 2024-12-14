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
    void testShowsAllFiles();
    void testShowsAllFiles_data();
    void testCurrentFilter();
    void testSetFilterWithDefault();
    void testAllSupportedFiles();
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
    combo.setFilters(KFileFilter::fromFilterString(filterString));

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
    combo.setDefaultFilter(KFileFilter::fromFilterString("*.cpp|Sources (*.cpp)").first());

    combo.setFilters({});
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "Sources (*.cpp)");

    combo.setFilters(KFileFilter::fromFilterString("*.png|PNG Image (*.png)"));
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "PNG Image (*.png)");

    combo.setFilters({});
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "Sources (*.cpp)");
}

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

    QList<KFileFilter> filters;

    for (const QString &mimeType : mimeTypes) {
        filters << KFileFilter::fromMimeType(mimeType);
    }

    combo.setFilters(filters, KFileFilter::fromMimeType(defaultType));

    QCOMPARE(combo.showsAllTypes(), expectedShowsAllFiles);
}

void KFileFilterComboTest::testCurrentFilter()
{
    KFileFilterCombo combo;

    const KFileFilter cppFilter("C++ Sources", {"*.cpp"}, {});
    const KFileFilter pngFilter("PNG Images", {"*.png"}, {});
    const KFileFilter pdfFilter("PDF Documents", {"*.pdf"}, {});

    combo.setFilters({cppFilter, pngFilter, pdfFilter});

    QCOMPARE(combo.currentFilter(), cppFilter);
    QCOMPARE(combo.currentIndex(), 0);

    combo.setCurrentFilter(pngFilter);
    QCOMPARE(combo.currentFilter(), pngFilter);
    QCOMPARE(combo.currentIndex(), 1);

    // User enters something custom
    combo.setCurrentText("*.md");
    QCOMPARE(combo.currentFilter(), KFileFilter("*.md", {"*.md"}, {}));

    combo.setCurrentText("*.c|C Sources");
    QCOMPARE(combo.currentFilter(), KFileFilter("C Sources", {"*.c"}, {}));

    // User enters something custom
    combo.setCurrentText("text/plain");
    QCOMPARE(combo.currentFilter(), KFileFilter::fromMimeType("text/plain"));
}

void KFileFilterComboTest::testSetFilterWithDefault()
{
    KFileFilterCombo combo;

    const KFileFilter cppFilter("C++ Sources", {"*.cpp"}, {});
    const KFileFilter pngFilter("PNG Images", {"*.png"}, {});
    const KFileFilter pdfFilter("PDF Documents", {"*.pdf"}, {});
    const KFileFilter allFilter("All Files", {}, {"application/octet-stream"});

    combo.setFilters({cppFilter, pngFilter, pdfFilter}, pngFilter);
    QCOMPARE(combo.currentFilter(), pngFilter);
    QCOMPARE(combo.currentText(), "PNG Images");

    combo.setFilters({allFilter, cppFilter, pngFilter}, allFilter);
    QCOMPARE(combo.currentFilter(), allFilter);
    QCOMPARE(combo.currentText(), "All Files");
}

void KFileFilterComboTest::testAllSupportedFiles()
{
    KFileFilterCombo combo;

    const KFileFilter cppFilter("C++ Sources", {}, {"text/x-c++src"});
    const KFileFilter pngFilter("PNG Images", {"*.png"}, {});
    const KFileFilter pdfFilter("PDF Documents", {"*.pdf"}, {});

    combo.setFilters({cppFilter, pngFilter, pdfFilter});

    // Check that it has the right "All supported types" filter
    QStringList expectedMimeTypes{"text/x-c++src"};
    QStringList expectedFileTypes{"*.png", "*.pdf"};
    QCOMPARE(combo.currentFilter().mimePatterns(), expectedMimeTypes);
    QCOMPARE(combo.currentFilter().filePatterns(), expectedFileTypes);
    QCOMPARE(combo.currentFilter().label(), "C++ Sources, PNG Images, PDF Documents");
}

QTEST_MAIN(KFileFilterComboTest)
#include "kfilefiltercombotest.moc"
