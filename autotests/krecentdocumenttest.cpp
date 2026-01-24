/*
    SPDX-FileCopyrightText: 2022 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "krecentdocumenttest.h"
#include "kconfiggroup.h"
#include "ksharedconfig.h"

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
    const auto url = QUrl::fromLocalFile(m_testFile);

    KRecentDocument::add(url, QStringLiteral("my-application"));
    KRecentDocument::add(url, QStringLiteral("my-application-2"));
    KRecentDocument::add(url, QStringLiteral("my-application"));

    auto xbelFile = QFile(m_xbelPath);
    QVERIFY(xbelFile.open(QIODevice::OpenModeFlag::ReadOnly));
    auto xbelContent = xbelFile.readAll();
    xbelFile.close();

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
        QCOMPARE(applicationElement.attribute(QStringLiteral("exec")), QStringLiteral("krecentdocumenttest %f"));
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

void KRecentDocumentTest::testXbelBookmarkMaxEntries()
{
    KConfigGroup config = KSharedConfig::openConfig()->group(QStringLiteral("RecentDocuments"));
    config.writeEntry(QStringLiteral("UseRecent"), true);
    config.writeEntry(QStringLiteral("MaxEntries"), 3);

    auto currentPath = QDir::currentPath();
    QStringList tempFiles;
    for (int i = 0; i < 15; ++i) {
        QString fileName(QStringLiteral("%1/temp File %2").arg(currentPath, QString::number(i)));
        QFile tempFile(fileName);
        QVERIFY(tempFile.open(QIODevice::WriteOnly));
        tempFile.close();

        KRecentDocument::add(QUrl::fromLocalFile(fileName), QStringLiteral("my-application"));

        tempFiles.push_back(fileName);
    }

    const auto recentUrls = KRecentDocument::recentUrls();
    QCOMPARE(recentUrls.length(), 3);

    for (int i = 0; i < 3; ++i) {
        QCOMPARE(recentUrls.at(i).fileName(), QStringLiteral("temp File %1").arg(QString::number(i + 12)));
    }

    for (const auto &fileName : tempFiles) {
        QFile::remove(fileName);
    }
}

void KRecentDocumentTest::testRemoveUrl()
{
    const auto url = QUrl::fromLocalFile(m_testFile);

    KRecentDocument::add(url, QStringLiteral("my-application"));
    KRecentDocument::add(url, QStringLiteral("my-application-2"));
    KRecentDocument::add(url, QStringLiteral("my-application"));

    // remove the url from the history
    KRecentDocument::removeFile(url);

    auto xbelFile = QFile(m_xbelPath);
    QVERIFY(xbelFile.open(QIODevice::OpenModeFlag::ReadOnly));
    auto xbelContent = xbelFile.readAll();
    xbelFile.close();

    QDomDocument reader;
    QVERIFY(reader.setContent(xbelContent));

    auto bookmarks = reader.elementsByTagName("bookmark");
    // check there isn't any bookmark left
    QCOMPARE(bookmarks.length(), 0);
}

void KRecentDocumentTest::testRemoveApplication()
{
    const auto url = QUrl::fromLocalFile(m_testFile);

    KRecentDocument::add(url, QStringLiteral("my-application"));
    KRecentDocument::add(url, QStringLiteral("my-application-2"));
    KRecentDocument::add(url, QStringLiteral("my-application"));

    auto checkNumberOfApplication = [this](int numberBookmarks, int numberApplications) {
        auto xbelFile = QFile(m_xbelPath);
        QVERIFY(xbelFile.open(QIODevice::OpenModeFlag::ReadOnly));
        auto xbelContent = xbelFile.readAll();
        xbelFile.close();

        QDomDocument reader;
        QVERIFY(reader.setContent(xbelContent));

        auto bookmarks = reader.elementsByTagName("bookmark");
        QCOMPARE(bookmarks.length(), numberBookmarks);

        auto applications = reader.elementsByTagName("bookmark:application");
        QCOMPARE(applications.length(), numberApplications);
    };

    // precondition, one bookmark with two applications
    checkNumberOfApplication(1, 2);

    // remove the application from the history
    KRecentDocument::removeApplication(QStringLiteral("my-application"));

    checkNumberOfApplication(1, 1);

    // remove the last application from the history
    KRecentDocument::removeApplication(QStringLiteral("my-application-2"));

    checkNumberOfApplication(0, 0);
}

void KRecentDocumentTest::testRemoveBookmarksModifiedSince()
{
    const auto url = QUrl::fromLocalFile(m_testFile);

    KRecentDocument::add(url, QStringLiteral("my-application"));
    KRecentDocument::add(url, QStringLiteral("my-application-2"));
    KRecentDocument::add(url, QStringLiteral("my-application"));

    KRecentDocument::removeBookmarksModifiedSince(QDateTime::currentDateTime().addSecs(-10));

    auto xbelFile = QFile(m_xbelPath);
    QVERIFY(xbelFile.open(QIODevice::OpenModeFlag::ReadOnly));
    auto xbelContent = xbelFile.readAll();
    xbelFile.close();

    QDomDocument reader;
    QVERIFY(reader.setContent(xbelContent));

    auto bookmarks = reader.elementsByTagName("bookmark");
    // check there isn't any bookmark left
    QCOMPARE(bookmarks.length(), 0);
}

QTEST_MAIN(KRecentDocumentTest)

#include "moc_krecentdocumenttest.cpp"
