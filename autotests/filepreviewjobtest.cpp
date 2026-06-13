/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Meven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filepreviewjobtest.h"

#include "../src/gui/filepreviewjob.h"

#include <KFileItem>
#include <KIO/PreviewJob>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>

QTEST_GUILESS_MAIN(FilePreviewJobTest)

using namespace KIO;

namespace
{
// Creates a regular file with some content and returns a KFileItem for it.
KFileItem makeFileItem(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("some content");
        file.close();
    }
    return KFileItem(QUrl::fromLocalFile(path));
}
}

// Computes the on-disk cache path that PreviewJob::cachedThumbnail() looks up
// for the given source url at the requested size, reusing the production path
// derivation so the test and the lookup cannot drift apart.
QString FilePreviewJobTest::cacheThumbnailPath(const QUrl &url, const QSize &size, qreal dpr)
{
    const QByteArray uri = url.toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded);
    const QString thumbRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
    return FilePreviewJob::thumbnailCachePath(uri, thumbRoot, size, dpr);
}

// Writes a thumbnail into the cache for the given source url at size/dpr,
// stamped with the given freedesktop metadata.
void FilePreviewJobTest::writeCachedThumbnail(const QUrl &url, const QSize &size, qreal dpr, qint64 mtimeSecs, qint64 fileSize)
{
    const QString path = cacheThumbnailPath(url, size, dpr);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QImage thumb(64, 64, QImage::Format_ARGB32);
    thumb.fill(Qt::blue);
    thumb.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(url.toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded)));
    thumb.setText(QStringLiteral("Thumb::MTime"), QString::number(mtimeSecs));
    thumb.setText(QStringLiteral("Thumb::Size"), QString::number(fileSize));
    thumb.save(path, "png");
}

void FilePreviewJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void FilePreviewJobTest::testTimeoutTimerStoppedOnFinish()
{
    // Regression test: FilePreviewJob's timeout timer used to keep running after
    // the job had finished. If the job's deferred deletion was stalled behind a
    // nested event loop (e.g. an open context menu via QMenu::exec), the timer
    // would later fire on the already-finished job and crash in KJob::kill().

    QTemporaryDir thumbRoot;
    QVERIFY(thumbRoot.isValid());

    QTemporaryFile file;
    QVERIFY(file.open());
    file.write("test");
    file.close();

    const KFileItem item(QUrl::fromLocalFile(file.fileName()));

    PreviewOptions options;
    options.size = QSize(128, 128);

    PreviewSetupData setupData;
    setupData.thumbRoot = thumbRoot.path();
    // Leave pluginByMimeTable empty on purpose: with no thumbnailer plugin the
    // job finishes quickly, which is all we need to exercise the start()->finish
    // lifecycle of the timeout timer (no thumbnail worker required).

    auto *job = new FilePreviewJob(item, FilePreviewJob::UnknownDeviceId, options, setupData);
    job->setAutoDelete(false);

    QSignalSpy resultSpy(job, &KJob::result);
    job->start();

    QVERIFY(resultSpy.wait());
    QCOMPARE(resultSpy.count(), 1);

    // The job finished well within the 5s timeout interval, so the only reason
    // the single-shot timer can be inactive is that finishing stopped it.
    QVERIFY(job->m_timeoutTimer);
    QVERIFY(!job->m_timeoutTimer->isActive());

    // It must also not fire afterwards: spinning the event loop must not produce
    // a second result (the old code re-entered emitResult() via slotTimeout()).
    QTest::qWait(50);
    QCOMPARE(resultSpy.count(), 1);

    delete job;
}

void FilePreviewJobTest::testCachedThumbnailReturnsFreshThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));

    const QSize size(256, 256);
    writeCachedThumbnail(item.targetUrl(), size, 1.0, item.time(KFileItem::ModificationTime).toSecsSinceEpoch(), item.size());

    const QImage thumb = PreviewJob::cachedThumbnail(item, size, 1.0);
    QVERIFY(!thumb.isNull());
    QVERIFY(PreviewJob::cachedThumbnailMatchesFile(thumb, item));
}

void FilePreviewJobTest::testCachedThumbnailReturnsStaleThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));

    const QSize size(256, 256);
    // The cached thumbnail records an older modification time than the file has.
    const qint64 staleMTime = item.time(KFileItem::ModificationTime).toSecsSinceEpoch() - 100;
    writeCachedThumbnail(item.targetUrl(), size, 1.0, staleMTime, item.size());

    // The stale thumbnail is still returned so it can be shown while a fresh one
    // is generated, but it is reported as no longer matching the file.
    const QImage thumb = PreviewJob::cachedThumbnail(item, size, 1.0);
    QVERIFY(!thumb.isNull());
    QVERIFY(!PreviewJob::cachedThumbnailMatchesFile(thumb, item));
}

void FilePreviewJobTest::testCachedThumbnailDetectsSizeMismatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));

    const QSize size(256, 256);
    // The recorded file size does not match the current file.
    writeCachedThumbnail(item.targetUrl(), size, 1.0, item.time(KFileItem::ModificationTime).toSecsSinceEpoch(), item.size() + 1);

    const QImage thumb = PreviewJob::cachedThumbnail(item, size, 1.0);
    QVERIFY(!thumb.isNull());
    QVERIFY(!PreviewJob::cachedThumbnailMatchesFile(thumb, item));
}

void FilePreviewJobTest::testCachedThumbnailMissReturnsNull()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));

    // Nothing was written to the cache.
    QVERIFY(PreviewJob::cachedThumbnail(item, QSize(256, 256), 1.0).isNull());
}

void FilePreviewJobTest::testCachedThumbnailWrongSizeBucket()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));

    // The thumbnail is cached for the 128px bucket only.
    writeCachedThumbnail(item.targetUrl(), QSize(128, 128), 1.0, item.time(KFileItem::ModificationTime).toSecsSinceEpoch(), item.size());

    // A request for the 256px bucket looks in a different directory and misses.
    QVERIFY(PreviewJob::cachedThumbnail(item, QSize(256, 256), 1.0).isNull());
    // The matching request finds it.
    QVERIFY(!PreviewJob::cachedThumbnail(item, QSize(128, 128), 1.0).isNull());
}

void FilePreviewJobTest::testCachedThumbnailRejectsIneligibleItems()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QSize size(256, 256);

    // A directory has no single cached thumbnail file.
    const KFileItem dirItem(QUrl::fromLocalFile(dir.path()));
    QVERIFY(PreviewJob::cachedThumbnail(dirItem, size, 1.0).isNull());

    // A remote item cannot be looked up in the local cache by this synchronous path.
    const KFileItem remoteItem(QUrl(QStringLiteral("ftp://example.com/photo.png")));
    QVERIFY(PreviewJob::cachedThumbnail(remoteItem, size, 1.0).isNull());

#ifndef Q_OS_WIN
    // A symlink's thumbnail is keyed by its target, so it is left to a full job.
    // Skipped on Windows, where QFile::link() creates a .lnk shortcut that
    // KFileItem does not report as a symlink.
    const QString target = dir.filePath(QStringLiteral("target.png"));
    makeFileItem(target);
    const QString link = dir.filePath(QStringLiteral("link.png"));
    QVERIFY(QFile::link(target, link));
    const KFileItem linkItem(QUrl::fromLocalFile(link));
    QVERIFY(linkItem.isLink());
    writeCachedThumbnail(linkItem.targetUrl(), size, 1.0, linkItem.time(KFileItem::ModificationTime).toSecsSinceEpoch(), linkItem.size());
    QVERIFY(PreviewJob::cachedThumbnail(linkItem, size, 1.0).isNull());
#endif
}
