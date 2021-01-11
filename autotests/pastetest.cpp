/*
    This file is part of KDE
    SPDX-FileCopyrightText: 2005-2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "pastetest.h"

#include <QTest>
#include <QDir>
#include <QMimeData>
#include <QStandardPaths>
#include <QUrl>
#include <QSignalSpy>

#include <KUrlMimeData>
#include <kio/paste.h>
#include <kio/pastejob.h>
#include <KFileItem>

QTEST_MAIN(KIOPasteTest)

void KIOPasteTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");

    QVERIFY(m_tempDir.isValid());
    m_dir = m_tempDir.path();
}

void KIOPasteTest::testPopulate()
{
    QMimeData *mimeData = new QMimeData;

    // Those URLs don't have to exist.
    QUrl mediaURL(QStringLiteral("media:/hda1/tmp/Mat%C3%A9riel"));
    QUrl localURL(QStringLiteral("file:///tmp/Mat%C3%A9riel"));
    QList<QUrl> kdeURLs; kdeURLs << mediaURL;
    QList<QUrl> mostLocalURLs; mostLocalURLs << localURL;

    KUrlMimeData::setUrls(kdeURLs, mostLocalURLs, mimeData);

    QVERIFY(mimeData->hasUrls());
    const QList<QUrl> lst = KUrlMimeData::urlsFromMimeData(mimeData);
    QCOMPARE(lst.count(), 1);
    QCOMPARE(lst[0].url(), mediaURL.url());

    const bool isCut = KIO::isClipboardDataCut(mimeData);
    QVERIFY(!isCut);

    delete mimeData;
}

void KIOPasteTest::testCut()
{
    QMimeData *mimeData = new QMimeData;

    QUrl localURL1(QStringLiteral("file:///tmp/Mat%C3%A9riel"));
    QUrl localURL2(QStringLiteral("file:///tmp"));
    QList<QUrl> localURLs; localURLs << localURL1 << localURL2;

    KUrlMimeData::setUrls(QList<QUrl>(), localURLs, mimeData);
    KIO::setClipboardDataCut(mimeData, true);

    QVERIFY(mimeData->hasUrls());
    const QList<QUrl> lst = KUrlMimeData::urlsFromMimeData(mimeData);
    QCOMPARE(lst.count(), 2);
    QCOMPARE(lst[0].url(), localURL1.url());
    QCOMPARE(lst[1].url(), localURL2.url());

    const bool isCut = KIO::isClipboardDataCut(mimeData);
    QVERIFY(isCut);

    delete mimeData;
}

void KIOPasteTest::testPasteActionText_data()
{
    QTest::addColumn<QList<QUrl> >("urls");
    QTest::addColumn<bool>("data");
    QTest::addColumn<bool>("expectedEnabled");
    QTest::addColumn<QString>("expectedText");

    QList<QUrl> urlDir = QList<QUrl>{QUrl::fromLocalFile(QDir::tempPath())};
    QList<QUrl> urlFile = QList<QUrl>{QUrl::fromLocalFile(QCoreApplication::applicationFilePath())};
    QList<QUrl> urlRemote = QList<QUrl>{QUrl(QStringLiteral("http://www.kde.org"))};
    QList<QUrl> urls = urlDir + urlRemote;
    QTest::newRow("nothing") << QList<QUrl>() << false << false << "Paste";
    QTest::newRow("one_dir") << urlDir << false << true << "Paste One Folder";
    QTest::newRow("one_file") << urlFile << false << true << "Paste One File";
    QTest::newRow("one_url") << urlRemote << false << true << "Paste One Item";
    QTest::newRow("two_urls") << urls << false << true << "Paste 2 Items";
    QTest::newRow("data") << QList<QUrl>() << true << true << "Paste Clipboard Contents...";
}

void KIOPasteTest::testPasteActionText()
{
    QFETCH(QList<QUrl>, urls);
    QFETCH(bool, data);
    QFETCH(bool, expectedEnabled);
    QFETCH(QString, expectedText);

    QMimeData mimeData;
    if (!urls.isEmpty()) {
        mimeData.setUrls(urls);
    }
    if (data) {
        mimeData.setText(QStringLiteral("foo"));
    }
    QCOMPARE(KIO::canPasteMimeData(&mimeData), expectedEnabled);
    bool canPaste;
    KFileItem destItem(QUrl::fromLocalFile(QDir::homePath()));
    QCOMPARE(KIO::pasteActionText(&mimeData, &canPaste, destItem), expectedText);
    QCOMPARE(canPaste, expectedEnabled);

    KFileItem nonWritableDestItem(QUrl::fromLocalFile(QStringLiteral("/nonwritable")));
    QCOMPARE(KIO::pasteActionText(&mimeData, &canPaste, nonWritableDestItem), expectedText);
    QCOMPARE(canPaste, false);

    KFileItem emptyUrlDestItem = KFileItem(QUrl());
    QCOMPARE(KIO::pasteActionText(&mimeData, &canPaste, emptyUrlDestItem), expectedText);
    QCOMPARE(canPaste, false);

    KFileItem nullDestItem;
    QCOMPARE(KIO::pasteActionText(&mimeData, &canPaste, nullDestItem), expectedText);
    QCOMPARE(canPaste, false);
}

static void createTestFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qFatal("Couldn't create %s", qPrintable(path));
    }
    QByteArray data("Hello world", 11);
    QCOMPARE(data.size(), 11);
    f.write(data);
}

void KIOPasteTest::testPasteJob_data()
{
    QTest::addColumn<QList<QUrl> >("urls");
    QTest::addColumn<bool>("data");
    QTest::addColumn<bool>("cut");
    QTest::addColumn<QString>("expectedFileName");

    const QString file = m_dir + "/file";
    createTestFile(file);

    QList<QUrl> urlFile = QList<QUrl>{QUrl::fromLocalFile(file)};
    QList<QUrl> urlDir = QList<QUrl>{QUrl::fromLocalFile(m_dir)};

    QTest::newRow("nothing") << QList<QUrl>() << false << false << QString();
    QTest::newRow("copy_one_file") << urlFile << false << false << file.section('/', -1);
    QTest::newRow("copy_one_dir") << urlDir << false << false << m_dir.section('/', -1);
    QTest::newRow("cut_one_file") << urlFile << false << true << file.section('/', -1);
    QTest::newRow("cut_one_dir") << urlDir << false << true << m_dir.section('/', -1);

    // Shows a dialog!
    //QTest::newRow("data") << QList<QUrl>() << true << "output_file";
}

void KIOPasteTest::testPasteJob()
{
    QFETCH(QList<QUrl>, urls);
    QFETCH(bool, data);
    QFETCH(bool, cut);
    QFETCH(QString, expectedFileName);

    QMimeData mimeData;
    bool isDir = false;
    bool isFile = false;
    if (!urls.isEmpty()) {
        mimeData.setUrls(urls);
        QFileInfo fileInfo(urls.first().toLocalFile());
        isDir = fileInfo.isDir();
        isFile = fileInfo.isFile();
    }
    if (data) {
        mimeData.setText(QStringLiteral("Hello world"));
    }
    KIO::setClipboardDataCut(&mimeData, cut);

    QTemporaryDir destTempDir;
    QVERIFY(destTempDir.isValid());
    const QString destDir = destTempDir.path();
    KIO::PasteJob *job = KIO::paste(&mimeData, QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    QSignalSpy spy(job, &KIO::PasteJob::itemCreated);
    QVERIFY(spy.isValid());
    job->setUiDelegate(nullptr);
    const bool expectedSuccess = !expectedFileName.isEmpty();
    QCOMPARE(job->exec(), expectedSuccess);
    if (expectedSuccess) {
        const QString destFile = destDir + '/' + expectedFileName;
        QVERIFY2(QFile::exists(destFile), qPrintable(expectedFileName));
        if (isDir) {
            QVERIFY(QFileInfo(destFile).isDir());
        } else {
            QVERIFY(QFileInfo(destFile).isFile());
            QFile file(destFile);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(QString(file.readAll()), QString("Hello world"));
        }
        if (cut) {
            QVERIFY(!QFile::exists(urls.first().toLocalFile()));
        } else {
            QVERIFY(QFile::exists(urls.first().toLocalFile()));
        }
        QCOMPARE(spy.count(), isFile || cut ? 1 : 2);
        QCOMPARE(spy.at(0).at(0).value<QUrl>().toLocalFile(), destFile);
    }
}
