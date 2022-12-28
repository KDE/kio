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
    void testSetMimeFilter();
    void testSetMimeFilter_data();
    void testIsMimeFilter();
    void testSetMimeFilterDefault();
    void testSetMimeFilterDefault_data();
    void testFilters();
    void testFilters_data();
    void testFiltersMime();
    void testFiltersMime_data();
    void testShowsAllFiles();
    void testShowsAllFiles_data();
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
    combo.setFilter(filterString);

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

    // must handle an unescaped slash https://bugs.kde.org/show_bug.cgi?id=463309
    QTest::addRow("slash") << "*.c *.cpp|C/C++ Files" << QStringList{"C/C++ Files"};

    QString k3bFilter =
        "*|All Files\naudio/x-mp3 audio/x-wav application/x-ogg |Sound Files\naudio/x-wav |Wave Sound Files\naudio/x-mp3 |MP3 Sound Files\napplication/x-ogg "
        "|Ogg Vorbis Sound Files\nvideo/mpeg |MPEG Video Files";
    QTest::addRow("k3b") << k3bFilter
                         << QStringList{"All Files", "Sound Files", "Wave Sound Files", "MP3 Sound Files", "Ogg Vorbis Sound Files", "MPEG Video Files"};
}

void KFileFilterComboTest::testDefaultFilter()
{
    KFileFilterCombo combo;
    combo.setDefaultFilter("*.cpp|Sources (*.cpp)");

    combo.setFilter(QString());
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "Sources (*.cpp)");

    combo.setFilter("*.png|PNG Image (*.png)");
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "PNG Image (*.png)");

    combo.setFilter(QString());
    QCOMPARE(combo.itemData(0, Qt::DisplayRole).toString(), "Sources (*.cpp)");
}

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

    combo.setMimeFilter(mimeTypes, defaultType);

    QCOMPARE(combo.currentText(), expectedComboboxCurrentText);
}

void KFileFilterComboTest::testIsMimeFilter()
{
    KFileFilterCombo combo;

    combo.setFilter("*.png");
    QVERIFY(!combo.isMimeFilter());

    combo.setMimeFilter(QStringList{"image/jpeg"}, QString());
    QVERIFY(combo.isMimeFilter());
}

void KFileFilterComboTest::testFilters_data()
{
    QTest::addColumn<QString>("filterString");
    QTest::addColumn<QStringList>("expectedFilters");

    QTest::addRow("extension_name") << "*.cpp|Sources (*.cpp)" << QStringList{"*.cpp|Sources (*.cpp)"};
    QTest::addRow("multiple_filter") << "*.cpp|Sources (*.cpp)\n*.h|Header files" << QStringList{"*.cpp|Sources (*.cpp)", "*.h|Header files"};
    QTest::addRow("mutiple_extension_multiple_filter")
        << "*.cpp *.cc *.C|C++ Source Files\n*.h *.H|Header files" << QStringList{"*.cpp *.cc *.C|C++ Source Files", "*.h *.H|Header files"};
    QTest::addRow("pattern_only") << "*.cpp" << QStringList{"*.cpp"};

    QString k3bFilter =
        "*|All Files\naudio/x-mp3 audio/x-wav application/x-ogg |Sound Files\naudio/x-wav |Wave Sound Files\naudio/x-mp3 |MP3 Sound Files\napplication/x-ogg "
        "|Ogg Vorbis Sound Files\nvideo/mpeg |MPEG Video Files";
    QTest::addRow("k3b") << k3bFilter
                         << QStringList{"*|All Files",
                                        "audio/x-mp3 audio/x-wav application/x-ogg |Sound Files",
                                        "audio/x-wav |Wave Sound Files",
                                        "audio/x-mp3 |MP3 Sound Files",
                                        "application/x-ogg |Ogg Vorbis Sound Files",
                                        "video/mpeg |MPEG Video Files"};
}

void KFileFilterComboTest::testFilters()
{
    QFETCH(QString, filterString);
    QFETCH(QStringList, expectedFilters);

    KFileFilterCombo combo;

    combo.setFilter(filterString);

    QCOMPARE(combo.filters(), expectedFilters);
}

void KFileFilterComboTest::testFiltersMime_data()
{
    QTest::addColumn<QStringList>("mimeTypes");
    QTest::addColumn<QStringList>("expectedFilters");

    QTest::addRow("one") << QStringList{"image/png"} << QStringList{"image/png"};
    QTest::addRow("two") << QStringList{"image/png", "image/jpeg"} << QStringList{"image/png image/jpeg", "image/png", "image/jpeg"};
    QTest::addRow("four") << QStringList{"image/png", "image/jpeg", "text/calendar", "application/gzip"}
                          << QStringList{"image/png image/jpeg text/calendar application/gzip", "image/png", "image/jpeg", "text/calendar", "application/gzip"};
}

void KFileFilterComboTest::testFiltersMime()
{
    QFETCH(QStringList, mimeTypes);
    QFETCH(QStringList, expectedFilters);

    KFileFilterCombo combo;

    combo.setMimeFilter(mimeTypes, QString());

    QCOMPARE(combo.filters(), expectedFilters);
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

    combo.setMimeFilter(mimeTypes, defaultType);

    QCOMPARE(combo.showsAllTypes(), expectedShowsAllFiles);
}

QTEST_MAIN(KFileFilterComboTest)
#include "kfilefiltercombotest.moc"
