/* This file is part of KDE
    Copyright (c) 2013 Dawit Alemayehu <adawit@kde.org>

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

#include <QtTestWidgets>

#include "clipboardupdatertest.h"
#include "kiotesthelper.h"
#include "clipboardupdater_p.h"

#include <kio/job.h>
#include <kio/paste.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>

#include <QTemporaryDir>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>


QTEST_MAIN(ClipboardUpdaterTest)

using namespace KIO;

static QList<QUrl> tempFiles(const QTemporaryDir& dir, const QString& baseName, int count = 3)
{
    QList<QUrl> urls;
    const QString path = dir.path();
    for (int i = 1; i < count+1; ++i) {
        const QString file = (path + '/' + baseName + QString::number(i));
        urls << QUrl::fromLocalFile(file);
        createTestFile(file);
    }
    return urls;
}

void ClipboardUpdaterTest::testPasteAfterRenameFiles()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QLatin1String("rfile"));

    QClipboard* clipboard = QApplication::clipboard();
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    Q_FOREACH(const QUrl& url, urls) {
        QUrl newUrl = url;
        newUrl.setPath(url.path() + QLatin1String("_renamed"));
        KIO::SimpleJob* job = KIO::rename(url, newUrl, KIO::HideProgressInfo);
        QVERIFY(job->exec());
    }

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job* job = KIO::pasteClipboard(QUrl::fromLocalFile(pasteDir), 0);
    QVERIFY(job->exec());
    QCOMPARE(job->error(), 0);
}

void ClipboardUpdaterTest::testPasteAfterMoveFile()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QLatin1String("mfile"), 1);

    QClipboard* clipboard = QApplication::clipboard();
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    const QString moveDir = dir.path() + QLatin1String("/movedir/");
    createTestDirectory(moveDir, NoSymlink);
    const QUrl srcUrl = urls.first();
    QUrl destUrl = QUrl::fromLocalFile(moveDir);
    destUrl = destUrl.adjusted(QUrl::RemoveFilename);
    destUrl.setPath(destUrl.path() + srcUrl.fileName());
    KIO::FileCopyJob* mJob = KIO::file_move(srcUrl, destUrl, -1, KIO::HideProgressInfo);
    QVERIFY(mJob->exec());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job* job = KIO::pasteClipboard(QUrl::fromLocalFile(pasteDir), 0);
    QVERIFY(job->exec());
    QCOMPARE(job->error(), 0);
}

void ClipboardUpdaterTest::testPasteAfterMoveFiles()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QLatin1String("mfile"));

    QClipboard* clipboard = QApplication::clipboard();
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    const QString moveDir = dir.path() + QLatin1String("/movedir");
    createTestDirectory(moveDir, NoSymlink);
    KIO::CopyJob* mJob = KIO::move(urls, QUrl::fromLocalFile(moveDir), KIO::HideProgressInfo);
    QVERIFY(mJob->exec());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job* job = KIO::pasteClipboard(QUrl::fromLocalFile(pasteDir), 0);
    QVERIFY(job->exec());
    QCOMPARE(job->error(), 0);
}

void ClipboardUpdaterTest::testPasteAfterDeleteFile()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QLatin1String("dfile"), 1);

    QClipboard* clipboard = QApplication::clipboard();
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    SimpleJob* sJob = KIO::file_delete(urls.first(), KIO::HideProgressInfo);
    QVERIFY(sJob->exec());

    QVERIFY(!clipboard->mimeData()->hasUrls());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job* job = KIO::pasteClipboard(QUrl::fromLocalFile(pasteDir), 0);
    QVERIFY(!job);
}

void ClipboardUpdaterTest::testPasteAfterDeleteFiles()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QLatin1String("dfile"));

    QClipboard* clipboard = QApplication::clipboard();
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    DeleteJob* dJob = KIO::del(urls, KIO::HideProgressInfo);
    QVERIFY(dJob->exec());

    QVERIFY(!clipboard->mimeData()->hasUrls());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job* job = KIO::pasteClipboard(QUrl::fromLocalFile(pasteDir), 0);
    QVERIFY(!job);
}

