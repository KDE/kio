/*
    SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kurlnavigatortest.h"
#include <KFilePlacesModel>
#include <KUser>
#include <QDir>
#include <QPushButton>
#include <QStandardPaths>
#include <QtTestWidgets>

#include "kiotesthelper.h" // createTestDirectory(), createTestSymlink()
#include "kurlcombobox.h"
#include "kurlnavigator.h"
#include <kprotocolinfo.h>

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

/*!
 * When the current URL is inside an archive and the user goes "up", it is expected
 * that the new URL is that of the folder containing the archive (unless the URL was
 * in a subfolder inside the archive). Furthermore, the protocol should be "file".
 * An empty protocol would lead to problems in Dolphin, see
 *
 * https://bugs.kde.org/show_bug.cgi?id=251553
 */

void KUrlNavigatorTest::bug251553_goUpFromArchive()
{
    // TODO: write a dummy archive protocol handler to mock things in the test
    // or consider making kio_archive not a "kio-extra", but a default kio plugin
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("zip"))) {
        QSKIP("No zip protocol support installed (e.g. kio_archive or kio_krarc)");
    }

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
    const QString dirC = tempDirPath + QLatin1String("/.c");
    const QString link = tempDirPath + QLatin1String("/l");
    createTestDirectory(dirA);
    createTestDirectory(dirB);
    createTestDirectory(dirC);
    createTestSymlink(link, dirA.toLatin1());

    QVERIFY(QFile::exists(dirA));
    QVERIFY(QFile::exists(dirB));
    QVERIFY(QFile::exists(dirC));
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

    // Replace all the text with ".c"
    m_navigator->editor()->setCurrentText(QStringLiteral(".c"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), QUrl::fromLocalFile(dirC));

    // Back to tempDir
    m_navigator->setLocationUrl(tempDirUrl);
    QCOMPARE(m_navigator->locationUrl(), tempDirUrl);

    // Replace all the text with "/a" - make sure this is handled as absolute path
    m_navigator->editor()->setCurrentText(QStringLiteral("/a"));
    QTest::keyClick(m_navigator->editor(), Qt::Key_Enter);
    QTRY_COMPARE(m_navigator->locationUrl(), QUrl::fromLocalFile("/a"));

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

#include "moc_kurlnavigatortest.cpp"
