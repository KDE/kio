/*
    SPDX-FileCopyrightText: 2015 Alejandro Fiestas Olivares <afiestas@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kurlcomboboxtest.h"
#include "kurlcombobox.h"

#include <QtTestWidgets>

QTEST_MAIN(KUrlComboBoxTest)

void KUrlComboBoxTest::testTextForItem_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("with_host") << "ftp://foo.com/folder" << "ftp://foo.com/folder/";
    QTest::newRow("with_no_host") << "smb://" << "smb://";
    QTest::newRow("with_host_without_path") << "ftp://user@example.com" << "ftp://user@example.com";
}

void KUrlComboBoxTest::testTextForItem()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedText);

    KUrlComboBox combo(KUrlComboBox::Directories);
    combo.setUrl(QUrl(url));

    QCOMPARE(combo.itemText(0), expectedText);

}

void KUrlComboBoxTest::testSetUrlMultipleTimes()
{
    KUrlComboBox combo(KUrlComboBox::Directories);
    combo.setUrl(QUrl("http://kde.org"));
    combo.setUrl(QUrl("http://www.kde.org"));
}

void KUrlComboBoxTest::testRemoveUrl()
{
    KUrlComboBox combo(KUrlComboBox::Both);
    combo.addDefaultUrl(QUrl("http://kde.org"));
    combo.addDefaultUrl(QUrl("http://www.kde.org"));

    QStringList urls{"http://foo.org", "http://bar.org"};
    combo.setUrls(urls);

    QCOMPARE(combo.urls(), urls);
    QCOMPARE(combo.count(), 4);
    QCOMPARE(combo.itemText(0), QString("http://kde.org"));
    QCOMPARE(combo.itemText(1), QString("http://www.kde.org"));
    QCOMPARE(combo.itemText(2), QString("http://foo.org"));
    QCOMPARE(combo.itemText(3), QString("http://bar.org"));

    // Remove a url
    combo.removeUrl(QUrl("http://foo.org"));
    QCOMPARE(combo.count(), 3);
    QCOMPARE(combo.urls(), QStringList{"http://bar.org"});
    QCOMPARE(combo.itemText(0), QString("http://kde.org"));
    QCOMPARE(combo.itemText(1), QString("http://www.kde.org"));
    QCOMPARE(combo.itemText(2), QString("http://bar.org"));

    // Removing a default url with checkDefaultURLs=true removes the url
    combo.removeUrl(QUrl("http://kde.org"));
    QCOMPARE(combo.count(), 2);
    QCOMPARE(combo.urls(), QStringList{"http://bar.org"});
    QCOMPARE(combo.itemText(0), QString("http://www.kde.org"));
    QCOMPARE(combo.itemText(1), QString("http://bar.org"));

    // Removing a default url with checkDefaultURLs=false does not remove the url
    combo.removeUrl(QUrl("http://www.kde.org"), false);
    QCOMPARE(combo.count(), 2);
    QCOMPARE(combo.urls(), QStringList{"http://bar.org"});
    QCOMPARE(combo.itemText(0), QString("http://www.kde.org"));
    QCOMPARE(combo.itemText(1), QString("http://bar.org"));

    // Removing a non-existing url is a no-op
    combo.removeUrl(QUrl("http://www.foo.org"));
    QCOMPARE(combo.count(), 2);
    QCOMPARE(combo.urls(), QStringList{"http://bar.org"});
    QCOMPARE(combo.itemText(0), QString("http://www.kde.org"));
    QCOMPARE(combo.itemText(1), QString("http://bar.org"));

    // Remove the last user provided url
    combo.removeUrl(QUrl("http://bar.org"));
    QCOMPARE(combo.count(), 1);
    QCOMPARE(combo.urls(), QStringList{});
    QCOMPARE(combo.itemText(0), QString("http://www.kde.org"));

    // Remove the last url
    combo.removeUrl(QUrl("http://www.kde.org"));
    QCOMPARE(combo.count(), 0);
    QCOMPARE(combo.urls(), QStringList{});
    QCOMPARE(combo.itemText(0), QString());
}

void KUrlComboBoxTest::testAddUrls()
{
    // GIVEN
    KUrlComboBox combo(KUrlComboBox::Both);
    combo.addDefaultUrl(QUrl("http://kde.org"));
    combo.addDefaultUrl(QUrl("http://www.kde.org"));

    const QStringList urls{"http://foo.org", "http://bar.org"};
    combo.setUrls(urls);

    // WHEN
    const QString url("http://foo.org/newUrl");
    combo.setUrl(QUrl(url));

    // THEN
    QStringList expected = urls;
    expected << url;
    QCOMPARE(combo.urls(), expected);
}

void KUrlComboBoxTest::testSetMaxItems()
{
    // GIVEN
    KUrlComboBox combo(KUrlComboBox::Both);
    combo.addDefaultUrl(QUrl("http://kde.org"));
    combo.addDefaultUrl(QUrl("http://www.kde.org"));

    const QStringList urls{"http://foo.org", "http://bar.org", "http://example.org", "http://example2.org"};
    combo.setUrls(urls);

    QStringList expected = urls;
    QCOMPARE(combo.urls(), expected);

    // WHEN
    combo.setMaxItems(4); // includes the default URLs

    // THEN
    expected = expected.mid(2);
    QCOMPARE(combo.urls(), expected);

    // WHEN
    combo.setMaxItems(1); // no room for additional URLs

    // THEN
    QCOMPARE(combo.urls(), QStringList());
}
