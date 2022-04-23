/*
    SPDX-FileCopyrightText: 2022 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "krecentdocumenttest.h"

#include <KRecentDocument>

#include <QDomDocument>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTest>

void KRecentDocumentTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    m_xbelPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/recently-used.xbel");

    cleanup();

    // must be outside of /tmp
    QFile tempFile(QDir::currentPath() + "/temp File");
    if (!tempFile.open(QIODevice::WriteOnly)) {
        qFatal("Can't create %s", qPrintable(tempFile.fileName()));
    }
    m_testFile = tempFile.fileName();
    qDebug() << "test file" << m_testFile;
}

void KRecentDocumentTest::cleanupTestCase()
{
    QFile(m_testFile).remove();
}

void KRecentDocumentTest::cleanup()
{
    QFile(m_xbelPath).remove();

    QString recentDocDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/RecentDocuments/");
    QDir(recentDocDir).removeRecursively();
}

void KRecentDocumentTest::testXbelBookmark()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());

    const auto url = QUrl::fromLocalFile(m_testFile);
    qDebug() << "url=" << url;
    KRecentDocument::add(url, QStringLiteral("my-application"));
    KRecentDocument::add(url, QStringLiteral("my-application-2"));
    KRecentDocument::add(url, QStringLiteral("my-application"));

    auto xbelFile = QFile(m_xbelPath);
    QVERIFY(xbelFile.open(QIODevice::OpenModeFlag::ReadOnly));
    auto xbelContent = xbelFile.readAll();
    xbelFile.close();

    auto expected = R"foo(<?xml version="1.0" encoding="UTF-8"?>
<xbel version="1.0" xmlns:bookmark="http://www.freedesktop.org/standards/desktop-bookmarks" xmlns:mime="http://www.freedesktop.org/standards/shared-mime-info">)foo";
    // check basic formatting
    QVERIFY(xbelContent.startsWith(expected));

    QDomDocument reader;
    QVERIFY(reader.setContent(xbelContent));

    auto bookmarks = reader.elementsByTagName("bookmark");
    // check there is only one <bookmark> element and matches expected href
    QCOMPARE(bookmarks.length(), 1);
    QVERIFY(bookmarks.at(0).attributes().contains("href"));
    QCOMPARE(url.toString(QUrl::FullyEncoded), bookmarks.at(0).toElement().attribute("href"));

    const auto apps = reader.elementsByTagName("bookmark:application");
    QCOMPARE(apps.length(), 2);

    for (int i = 0; i < apps.count(); ++i) {
        const auto applicationElement = apps.at(i).toElement();
        if (applicationElement.attribute(QStringLiteral("name")) == QStringLiteral("my-application")) {
            QCOMPARE(applicationElement.attribute("count"), QStringLiteral("2"));
        } else {
            QCOMPARE(applicationElement.attribute("count"), QStringLiteral("1"));
        }
        QCOMPARE(applicationElement.attribute(QStringLiteral("exec")), QStringLiteral("krecentdocumenttest %u"));
    }

    auto urls = KRecentDocument::recentUrls();
    if (urls.length() != 1) {
        qWarning() << urls;
    }

    QCOMPARE(urls.length(), 1);
    QCOMPARE(urls.at(0), url);

    QFile tempJpegFile(QDir::currentPath() + "/tempFile.jpg");
    QVERIFY(tempJpegFile.open(QIODevice::WriteOnly));
    const auto imgFileUrl = QUrl::fromLocalFile(tempJpegFile.fileName());
    KRecentDocument::add(imgFileUrl, QStringLiteral("my-image-viewer"));

    urls = KRecentDocument::recentUrls();

    QCOMPARE(urls.length(), 2);
    QCOMPARE(urls.at(0), url);
    QCOMPARE(urls.at(1), imgFileUrl);

    QVERIFY(xbelFile.open(QIODevice::OpenModeFlag::ReadOnly));
    xbelContent = xbelFile.readAll();
    xbelFile.close();
    QVERIFY(reader.setContent(xbelContent));
    auto bookmarksGroups = reader.elementsByTagName("bookmark:groups");
    QCOMPARE(bookmarksGroups.length(), 1);
    QCOMPARE(bookmarksGroups.at(0).toElement().text(), QStringLiteral("Graphics"));

    QFile temparchiveFile(QDir::currentPath() + "/tempFile.zip");
    QVERIFY(temparchiveFile.open(QIODevice::WriteOnly));
    const auto archiveFileUrl = QUrl::fromLocalFile(temparchiveFile.fileName());
    KRecentDocument::add(archiveFileUrl, QStringLiteral("my-archive-viewer"), QList{KRecentDocument::RecentDocumentGroup::Archive});

    urls = KRecentDocument::recentUrls();

    QCOMPARE(urls.length(), 3);
    QCOMPARE(urls.at(0), url);
    QCOMPARE(urls.at(1), imgFileUrl);
    QCOMPARE(urls.at(2), archiveFileUrl);

    QVERIFY(xbelFile.open(QIODevice::OpenModeFlag::ReadOnly));
    xbelContent = xbelFile.readAll();
    QVERIFY(reader.setContent(xbelContent));
    bookmarksGroups = reader.elementsByTagName("bookmark:groups");
    QCOMPARE(bookmarksGroups.length(), 2);
    QCOMPARE(bookmarksGroups.at(1).toElement().text(), QStringLiteral("Archive"));
}

QTEST_MAIN(KRecentDocumentTest)
