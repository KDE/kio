/*
    SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kurlnavigatortest.h"
#include <QtTestWidgets>
#include <QDir>
#include <QPushButton>
#include <QStandardPaths>
#include <KUser>
#include <KFilePlacesModel>

#include "kurlnavigator.h"
#include "kurlcombobox.h"
#include "kiotesthelper.h" // createTestDirectory(), createTestSymlink()

QTEST_MAIN(KUrlNavigatorTest)

void KUrlNavigatorTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    m_navigator = new KUrlNavigator(nullptr, QUrl(QStringLiteral("file:///A")), nullptr);
}

void KUrlNavigatorTest::cleanupTestCase()
{
    delete m_navigator;
    m_navigator = nullptr;
}

void KUrlNavigatorTest::testHistorySizeAndIndex()
{
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 1);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///A")));

    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 1);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///B")));

    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 2);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///C")));

    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 3);
}

void KUrlNavigatorTest::testGoBack()
{
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 3);

    bool ok = m_navigator->goBack();

    QVERIFY(ok);
    QCOMPARE(m_navigator->historyIndex(), 1);
    QCOMPARE(m_navigator->historySize(), 3);

    ok = m_navigator->goBack();

    QVERIFY(ok);
    QCOMPARE(m_navigator->historyIndex(), 2);
    QCOMPARE(m_navigator->historySize(), 3);

    ok = m_navigator->goBack();

    QVERIFY(!ok);
    QCOMPARE(m_navigator->historyIndex(), 2);
    QCOMPARE(m_navigator->historySize(), 3);
}

void KUrlNavigatorTest::testGoForward()
{
    QCOMPARE(m_navigator->historyIndex(), 2);
    QCOMPARE(m_navigator->historySize(), 3);

    bool ok = m_navigator->goForward();

    QVERIFY(ok);
    QCOMPARE(m_navigator->historyIndex(), 1);
    QCOMPARE(m_navigator->historySize(), 3);

    ok = m_navigator->goForward();

    QVERIFY(ok);
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 3);

    ok = m_navigator->goForward();

    QVERIFY(!ok);
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 3);
}

void KUrlNavigatorTest::testHistoryInsert()
{
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 3);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///D")));

    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 4);

    bool ok = m_navigator->goBack();
    QVERIFY(ok);
    QCOMPARE(m_navigator->historyIndex(), 1);
    QCOMPARE(m_navigator->historySize(), 4);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///E")));
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 4);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///F")));
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 5);

    ok = m_navigator->goBack();
    QVERIFY(ok);
    ok = m_navigator->goBack();
    QVERIFY(ok);
    QCOMPARE(m_navigator->historyIndex(), 2);
    QCOMPARE(m_navigator->historySize(), 5);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///G")));

    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 4);

    // insert same URL as the current history index
    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///G")));
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 4);

    // insert same URL with a trailing slash as the current history index
    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///G/")));
    QCOMPARE(m_navigator->historyIndex(), 0);
    QCOMPARE(m_navigator->historySize(), 4);

    // jump to "C" and insert same URL as the current history index
    ok = m_navigator->goBack();
    QVERIFY(ok);
    QCOMPARE(m_navigator->historyIndex(), 1);
    QCOMPARE(m_navigator->historySize(), 4);

    m_navigator->setLocationUrl(QUrl(QStringLiteral("file:///C")));
    QCOMPARE(m_navigator->historyIndex(), 1);
    QCOMPARE(m_navigator->historySize(), 4);
}

/**
 * When the current URL is inside an archive and the user goes "up", it is expected
 * that the new URL is that of the folder containing the archive (unless the URL was
 * in a subfolder inside the archive). Furthermore, the protocol should be "file".
 * An empty protocol would lead to problems in Dolphin, see
 *
 * https://bugs.kde.org/show_bug.cgi?id=251553
 */

void KUrlNavigatorTest::bug251553_goUpFromArchive()
{
    m_navigator->setLocationUrl(QUrl(QStringLiteral("zip:/test/archive.zip")));
    QCOMPARE(m_navigator->locationUrl().path(), QLatin1String("/test/archive.zip"));
    QCOMPARE(m_navigator->locationUrl().scheme(), QLatin1String("zip"));

    bool ok = m_navigator->goUp();
    QVERIFY(ok);
    QCOMPARE(m_navigator->locationUrl().path(), QLatin1String("/test/"));
    QCOMPARE(m_navigator->locationUrl().scheme(), QLatin1String("file"));

    m_navigator->setLocationUrl(QUrl(QStringLiteral("tar:/test/archive.tar.gz")));
    QCOMPARE(m_navigator->locationUrl().path(), QLatin1String("/test/archive.tar.gz"));
    QCOMPARE(m_navigator->locationUrl().scheme(), QLatin1String("tar"));

    ok = m_navigator->goUp();
    QVERIFY(ok);
    QCOMPARE(m_navigator->locationUrl().path(), QLatin1String("/test/"));
    QCOMPARE(m_navigator->locationUrl().scheme(), QLatin1String("file"));
}

void KUrlNavigatorTest::testUrlParsing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QUrl>("url");
    // due to a bug in the KF5 porting input such as '/home/foo/.config' was parsed as 'http:///home/foo/.config/'.
    QTest::newRow("hiddenFile") << QStringLiteral("/home/foo/.config") << QUrl::fromLocalFile(QStringLiteral("/home/foo/.config"));
    // TODO: test this on windows: e.g. 'C:/foo/.config' or 'C:\foo\.config'
    QTest::newRow("homeDir") << QStringLiteral("~") << QUrl::fromLocalFile(QDir::homePath());
    KUser user(KUser::UseRealUserID);
    QTest::newRow("userHomeDir") << (QStringLiteral("~") + user.loginName()) << QUrl::fromLocalFile(user.homeDir());
}

void KUrlNavigatorTest::testUrlParsing()
{
    QFETCH(QString, input);
    QFETCH(QUrl, url);

    m_navigator->setLocationUrl(QUrl());
    m_navigator->setUrlEditable(true);
    m_navigator->editor()->setCurrentText(input);
    QCOMPARE(m_navigator->uncommittedUrl(), url);
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QCOMPARE(m_navigator->locationUrl(), url);
}

void KUrlNavigatorTest::testRelativePaths()
{
    QTemporaryDir tempDir;
    const QString tempDirPath = tempDir.path();
    const QString dirA = tempDirPath + QLatin1String("/a");
    const QString dirB = tempDirPath + QLatin1String("/a/b");
    const QString link = tempDirPath + QLatin1String("/l");
    createTestDirectory(dirA);
    createTestDirectory(dirB);
    createTestSymlink(link, dirA.toLatin1());

    QVERIFY(QFile::exists(dirA));
    QVERIFY(QFile::exists(dirB));
    QVERIFY(QFile::exists(link));

    const QUrl tempDirUrl = QUrl::fromLocalFile(tempDirPath);
    const QUrl dirAUrl = QUrl::fromLocalFile(dirA);
    const QUrl linkUrl = QUrl::fromLocalFile(link);

    // Change to tempDir
    m_navigator->setLocationUrl(tempDirUrl);
    m_navigator->setUrlEditable(true);
    QCOMPARE(m_navigator->locationUrl(), tempDirUrl);

    // QTRY_COMPARE because of waiting for the stat job in applyUncommittedUrl() to finish

    // Replace all the text with "a"
    m_navigator->editor()->setCurrentText(QStringLiteral("a"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), dirAUrl);

    // Replace all the text with "b"
    m_navigator->editor()->setCurrentText(QStringLiteral("b"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), QUrl::fromLocalFile(dirB));

    // Test "../", which should go up in the dir hierarchy
    m_navigator->editor()->setCurrentText(QStringLiteral("../"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl().adjusted(QUrl::StripTrailingSlash), dirAUrl);
    // Test "..", which should go up in the dir hierarchy
    m_navigator->editor()->setCurrentText(QStringLiteral(".."));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), tempDirUrl);

    // Back to tempDir
    m_navigator->setLocationUrl(tempDirUrl);
    QCOMPARE(m_navigator->locationUrl(), tempDirUrl);
    // Replace all the text with "l" which is a symlink to dirA
    m_navigator->editor()->setCurrentText(QStringLiteral("l"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), linkUrl);

    // Back to tempDir
    m_navigator->setLocationUrl(tempDirUrl);
    QCOMPARE(m_navigator->locationUrl(), tempDirUrl);
    // Replace all the text with "a/b"
    m_navigator->editor()->setCurrentText(QStringLiteral("a/b"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), QUrl::fromLocalFile(dirB));
    // Now got to l "../../l"
    m_navigator->editor()->setCurrentText(QStringLiteral("../../l"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), linkUrl);
}

void KUrlNavigatorTest::testFixUrlPath_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QUrl>("url");
    // ":local" KProtocols, a '/' is added so that the url "path" isn't empty
    QTest::newRow("trashKIO") << (QStringLiteral("trash:")) << QUrl(QStringLiteral("trash:/"));
    // QUrl setPath("/") results in "file:///"
    QTest::newRow("fileKIO") << (QStringLiteral("file:")) << QUrl(QStringLiteral("file:///"));
}

void KUrlNavigatorTest::testFixUrlPath()
{
    QFETCH(QString, input);
    QFETCH(QUrl, url);

    m_navigator->setLocationUrl(QUrl());
    m_navigator->setUrlEditable(true);
    m_navigator->editor()->setCurrentText(input);
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QCOMPARE(m_navigator->locationUrl(), url);
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(4, 5)
void KUrlNavigatorTest::testButtonUrl_data()
{
    QTest::addColumn<QUrl>("locationUrl");
    QTest::addColumn<int>("buttonIndex");
    QTest::addColumn<QUrl>("expectedButtonUrl");

    QTest::newRow("localPathButtonIndex3") << QUrl::fromLocalFile(QStringLiteral("/home/foo")) << 3 << QUrl::fromLocalFile(QStringLiteral("/home/foo")); // out of range
    QTest::newRow("localPathButtonIndex2") << QUrl::fromLocalFile(QStringLiteral("/home/foo")) << 2 << QUrl::fromLocalFile(QStringLiteral("/home/foo"));
    QTest::newRow("localPathButtonIndex1") << QUrl::fromLocalFile(QStringLiteral("/home/foo")) << 1 << QUrl::fromLocalFile(QStringLiteral("/home"));
    QTest::newRow("localPathButtonIndex0") << QUrl::fromLocalFile(QStringLiteral("/home/foo")) << 0 << QUrl::fromLocalFile(QStringLiteral("/"));

    QTest::newRow("networkPathButtonIndex1") << QUrl::fromUserInput(QStringLiteral("network:/konqi.local/share")) << 1 << QUrl::fromUserInput(QStringLiteral("network:/konqi.local"));
    QTest::newRow("networkPathButtonIndex0") << QUrl::fromUserInput(QStringLiteral("network:/konqi.local/share")) << 0 << QUrl::fromUserInput(QStringLiteral("network:/"));

    QTest::newRow("ftpPathButtonIndex1") << QUrl::fromUserInput(QStringLiteral("ftp://kde.org/home/foo")) << 1 << QUrl::fromUserInput(QStringLiteral("ftp://kde.org/home"));
    QTest::newRow("ftpPathButtonIndex0") << QUrl::fromUserInput(QStringLiteral("ftp://kde.org/home/foo")) << 0 << QUrl::fromUserInput(QStringLiteral("ftp://kde.org/"));

    // bug 354678
    QTest::newRow("localPathWithPercentage") << QUrl::fromLocalFile(QStringLiteral("/home/foo %/test")) << 2 << QUrl::fromLocalFile(QStringLiteral("/home/foo %"));
}

void KUrlNavigatorTest::testButtonUrl()
{
    QFETCH(QUrl, locationUrl);
    QFETCH(int, buttonIndex);
    QFETCH(QUrl, expectedButtonUrl);

    // PREPARE
    m_navigator->setLocationUrl(locationUrl);

    // WHEN
    const QUrl buttonUrl = m_navigator->url(buttonIndex);

    // THEN
    QCOMPARE(buttonUrl, expectedButtonUrl);
}
#endif

void KUrlNavigatorTest::testButtonText()
{
    KFilePlacesModel model;
    const QUrl url = QUrl::fromLocalFile(QDir::currentPath());
    model.addPlace("&Here", url);
    KUrlNavigator navigator(&model, url, nullptr);

    QList<QPushButton *> buttons = navigator.findChildren<QPushButton *>();
    const auto it = std::find_if(buttons.cbegin(), buttons.cend(), [](QPushButton *button) {
            return button->text() == QLatin1String("&Here");
            });
    QVERIFY(it != buttons.cend());
    QCOMPARE((*it)->property("plainText").toString(), QStringLiteral("Here"));
}

void KUrlNavigatorTest::testInitWithRedundantPathSeparators()
{
    KUrlNavigator temp_nav(nullptr, QUrl::fromLocalFile(QStringLiteral("/home/foo///test")), nullptr);

    const QUrl buttonUrl = temp_nav.locationUrl();

    QCOMPARE(buttonUrl, QUrl::fromLocalFile(QStringLiteral("/home/foo/test")));
}
