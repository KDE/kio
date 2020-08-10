/*
    This file is part of KDE
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "clipboardupdatertest.h"

#include <QtTestWidgets>

#include "kiotesthelper.h"
#include "clipboardupdater_p.h"

#include <kio/job.h>
#include <kio/paste.h>
#include <kio/pastejob.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>

#include <QTemporaryDir>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>

QTEST_MAIN(ClipboardUpdaterTest)

using namespace KIO;

static QList<QUrl> tempFiles(const QTemporaryDir &dir, const QString &baseName, int count = 3)
{
    QList<QUrl> urls;
    const QString path = dir.path();
    for (int i = 1; i < count + 1; ++i) {
        const QString file = (path + '/' + baseName + QString::number(i));
        urls << QUrl::fromLocalFile(file);
        createTestFile(file);
    }
    return urls;
}

void ClipboardUpdaterTest::initTestCase()
{
    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");
}

void ClipboardUpdaterTest::testPasteAfterRenameFiles()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QStringLiteral("rfile"));

    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    for (const QUrl &url : urls) {
        QUrl newUrl = url;
        newUrl.setPath(url.path() + QLatin1String("_renamed"));
        KIO::SimpleJob *job = KIO::rename(url, newUrl, KIO::HideProgressInfo);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
    }

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job *job = KIO::paste(clipboard->mimeData(), QUrl::fromLocalFile(pasteDir));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->error(), 0);
}

void ClipboardUpdaterTest::testPasteAfterMoveFile()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QStringLiteral("mfile"), 1);

    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    const QString moveDir = dir.path() + QLatin1String("/movedir/");
    createTestDirectory(moveDir, NoSymlink);
    const QUrl srcUrl = urls.first();
    QUrl destUrl = QUrl::fromLocalFile(moveDir);
    destUrl = destUrl.adjusted(QUrl::RemoveFilename);
    destUrl.setPath(destUrl.path() + srcUrl.fileName());
    KIO::FileCopyJob *mJob = KIO::file_move(srcUrl, destUrl, -1, KIO::HideProgressInfo);
    QVERIFY(mJob->exec());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job *job = KIO::paste(clipboard->mimeData(), QUrl::fromLocalFile(pasteDir));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->error(), 0);
}

void ClipboardUpdaterTest::testPasteAfterMoveFiles()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QStringLiteral("mfile"));

    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    const QString moveDir = dir.path() + QLatin1String("/movedir");
    createTestDirectory(moveDir, NoSymlink);
    KIO::CopyJob *mJob = KIO::move(urls, QUrl::fromLocalFile(moveDir), KIO::HideProgressInfo);
    QVERIFY(mJob->exec());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job *job = KIO::paste(clipboard->mimeData(), QUrl::fromLocalFile(pasteDir));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->error(), 0);
}

void ClipboardUpdaterTest::testPasteAfterDeleteFile()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QStringLiteral("dfile"), 1);

    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    SimpleJob *sJob = KIO::file_delete(urls.first(), KIO::HideProgressInfo);
    QVERIFY(sJob->exec());

    QVERIFY(!clipboard->mimeData()->hasUrls());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job *job = KIO::paste(clipboard->mimeData(), QUrl::fromLocalFile(pasteDir), KIO::DefaultFlags);
    QVERIFY(job);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), int(KIO::ERR_NO_CONTENT));
}

void ClipboardUpdaterTest::testPasteAfterDeleteFiles()
{
    QTemporaryDir dir;
    const QList<QUrl> urls = tempFiles(dir, QStringLiteral("dfile"));

    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);

    DeleteJob *dJob = KIO::del(urls, KIO::HideProgressInfo);
    QVERIFY(dJob->exec());

    QVERIFY(!clipboard->mimeData()->hasUrls());

    const QString pasteDir = dir.path() + QLatin1String("/pastedir");
    createTestDirectory(pasteDir, NoSymlink);
    KIO::Job *job = KIO::paste(clipboard->mimeData(), QUrl::fromLocalFile(pasteDir), KIO::DefaultFlags);
    QVERIFY(job);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), int(KIO::ERR_NO_CONTENT));
}

