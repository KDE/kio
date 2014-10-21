/* This file is part of KDE
    Copyright (c) 2005-2006 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <qtest.h>

#include <QDir>
#include <QUrl>
#include <QMimeData>
#include <kurlmimedata.h>
#include <kio/paste.h>
#include <KFileItem>
#include "pastetest.h"

QTEST_MAIN(KIOPasteTest)

void KIOPasteTest::testPopulate()
{
    QMimeData* mimeData = new QMimeData;

    // Those URLs don't have to exist.
    QUrl mediaURL("media:/hda1/tmp/Mat%C3%A9riel");
    QUrl localURL("file:///tmp/Mat%C3%A9riel");
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
    QMimeData* mimeData = new QMimeData;

    QUrl localURL1("file:///tmp/Mat%C3%A9riel");
    QUrl localURL2("file:///tmp");
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

    QList<QUrl> urlDir = QList<QUrl>() << QUrl::fromLocalFile(QDir::tempPath());
    QList<QUrl> urlFile = QList<QUrl>() << QUrl::fromLocalFile(QCoreApplication::applicationFilePath());
    QList<QUrl> urlRemote = QList<QUrl>() << QUrl("http://www.kde.org");
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
        mimeData.setText("foo");
    }
    QCOMPARE(KIO::canPasteMimeData(&mimeData), expectedEnabled);
    bool canPaste;
    KFileItem destItem(QUrl::fromLocalFile(QDir::homePath()));
    QCOMPARE(KIO::pasteActionText(&mimeData, &canPaste, destItem), expectedText);
    QCOMPARE(canPaste, expectedEnabled);

    KFileItem nonWritableDestItem(QUrl::fromLocalFile("/nonwritable"));
    QCOMPARE(KIO::pasteActionText(&mimeData, &canPaste, nonWritableDestItem), expectedText);
    QCOMPARE(canPaste, false);
}
