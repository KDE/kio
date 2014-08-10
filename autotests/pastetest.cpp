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

#include <QUrl>
#include <QMimeData>
#include <kurlmimedata.h>
#include <kio/paste.h>
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
