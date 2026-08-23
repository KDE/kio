/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Meven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filepreviewjobtest.h"

#include "../src/gui/filepreviewjob.h"
#include "../src/gui/standardthumbnailjob_p.h"
#include "../src/gui/thumbnailcache_p.h"

#include <KFileItem>
#include <KIO/UDSEntry>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QSize>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>

#include <sys/stat.h>

QTEST_GUILESS_MAIN(FilePreviewJobTest)

using namespace KIO;

// A cached-thumbnail stand-in: cachedW x cachedH px with the metadata isCacheValid()
// reads. origW/origH <= 0 omit the original-size tags.
static QImage makeCachedThumb(int cachedW, int cachedH, const QByteArray &origName, qint64 mtime, int origW, int origH)
{
    QImage thumb(cachedW, cachedH, QImage::Format_ARGB32);
    thumb.fill(Qt::black);
    thumb.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(origName));
    thumb.setText(QStringLiteral("Thumb::MTime"), QString::number(mtime));
    if (origW > 0) {
        thumb.setText(QStringLiteral("Thumb::Image::Width"), QString::number(origW));
    }
    if (origH > 0) {
        thumb.setText(QStringLiteral("Thumb::Image::Height"), QString::number(origH));
    }
    return thumb;
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

void FilePreviewJobTest::testCacheSizeValidation_data()
{
    QTest::addColumn<int>("cachedW");
    QTest::addColumn<int>("cachedH");
    QTest::addColumn<int>("origW");
    QTest::addColumn<int>("origH");
    QTest::addColumn<bool>("expectedValid");
    QTest::addColumn<QSize>("expectedOutputSize"); // emitted preview when the cache is accepted

    // The request below needs 256 * 1.75 == 448 px (longer edge).
    QTest::newRow("exact size") << 448 << 448 << 4000 << 3000 << true << QSize(448, 448);
    QTest::newRow("larger than needed") << 512 << 512 << 4000 << 3000 << true << QSize(448, 448); // downscaled
    // Too small for the current request, but the original is large enough to yield a
    // bigger thumbnail: reject so it is regenerated at full resolution.
    QTest::newRow("too small, large original") << 256 << 256 << 4000 << 3000 << false << QSize();
    // Too small, but the original is itself that small: keep it, regenerating would
    // not produce anything bigger and would just repeat on every request.
    QTest::newRow("too small, small original") << 256 << 200 << 256 << 200 << true << QSize(256, 200);
    // Too small, but no original-size metadata to prove a bigger one exists: keep it.
    QTest::newRow("too small, no metadata") << 256 << 256 << 0 << 0 << true << QSize(256, 256);
}

void FilePreviewJobTest::testCacheSizeValidation()
{
    QFETCH(int, cachedW);
    QFETCH(int, cachedH);
    QFETCH(int, origW);
    QFETCH(int, origH);
    QFETCH(bool, expectedValid);
    QFETCH(QSize, expectedOutputSize);

    QTemporaryDir thumbRoot;
    QVERIFY(thumbRoot.isValid());

    const KFileItem item(QUrl::fromLocalFile(QStringLiteral("/tmp/does-not-matter.png")));

    PreviewOptions options;
    options.size = QSize(256, 256);
    options.devicePixelRatio = 1.75; // needed longer edge = 256 * 1.75 = 448 px

    PreviewSetupData setupData;
    setupData.thumbRoot = thumbRoot.path();

    auto *job = new FilePreviewJob(item, FilePreviewJob::UnknownDeviceId, options, setupData);
    job->setAutoDelete(false);

    // Make isCacheValid()'s identity checks pass (matching URI and mtime, no
    // Thumb::Size and no thumbnailer plugin), so only the size logic is exercised.
    const QByteArray origName = "file:///tmp/does-not-matter.png";
    const qint64 mtime = 1000000;
    job->m_origName = origName;
    job->m_tOrig = QDateTime::fromSecsSinceEpoch(mtime);

    QImage thumb = makeCachedThumb(cachedW, cachedH, origName, mtime, origW, origH);
    thumb.setDevicePixelRatio(options.devicePixelRatio); // as loadThumbnailFromCache() sets it
    QCOMPARE(job->isCacheValid(thumb), expectedValid);

    if (expectedValid) {
        // An accepted cache is emitted as-is (only downscaled when larger than needed).
        job->emitPreview(thumb);
        QCOMPARE(job->previewImage().size(), expectedOutputSize);
    }

    delete job;
}

void FilePreviewJobTest::testGeneratedImageDevicePixelRatio_data()
{
    QTest::addColumn<qreal>("dpr");
    QTest::addColumn<QSize>("thumbSize"); // source thumbnail, in device pixels
    QTest::addColumn<QSize>("requestSize"); // PreviewOptions::size, in logical pixels
    QTest::addColumn<QSize>("expectedSize"); // emitted image, in device pixels

    // Source larger than requestSize * dpr, so emitPreview() scales it down to that.
    QTest::newRow("scaled down, 1.75x") << 1.75 << QSize(448, 448) << QSize(128, 128) << QSize(224, 224);
    QTest::newRow("scaled down, 2x") << 2.0 << QSize(512, 512) << QSize(128, 128) << QSize(256, 256);
    // Source already fits within requestSize * dpr, so it is passed through unscaled.
    QTest::newRow("not scaled, 1.75x") << 1.75 << QSize(100, 100) << QSize(256, 256) << QSize(100, 100);
}

void FilePreviewJobTest::testGeneratedImageDevicePixelRatio()
{
    QFETCH(qreal, dpr);
    QFETCH(QSize, thumbSize);
    QFETCH(QSize, requestSize);
    QFETCH(QSize, expectedSize);

    QTemporaryDir thumbRoot;
    QVERIFY(thumbRoot.isValid());

    const KFileItem item(QUrl::fromLocalFile(QStringLiteral("/tmp/does-not-matter.png")));

    PreviewOptions options;
    options.size = requestSize;
    options.devicePixelRatio = dpr;

    PreviewSetupData setupData;
    setupData.thumbRoot = thumbRoot.path();

    auto *job = new FilePreviewJob(item, FilePreviewJob::UnknownDeviceId, options, setupData);
    job->setAutoDelete(false);

    QImage thumb(thumbSize, QImage::Format_ARGB32);
    thumb.fill(Qt::black);
    thumb.setDevicePixelRatio(dpr);

    // emitPreview() produces the image delivered by PreviewJob::generated(); it must
    // carry the device pixel ratio and be scaled to the requested device-pixel size.
    job->emitPreview(thumb);
    QCOMPARE(job->previewImage().devicePixelRatio(), dpr);
    QCOMPARE(job->previewImage().size(), expectedSize);

    delete job;
}

void FilePreviewJobTest::testSupportedDevicePixelRatio_data()
{
    QTest::addColumn<QSize>("imageSize");
    QTest::addColumn<qreal>("expectedDpr");

    // Shown at logical size 128 with a max ratio of 1.75, so the full-resolution target is
    // 224 px. The claimed ratio must be what the image resolution supports, floored at 1
    // and capped at 1.75.
    QTest::newRow("sufficient") << QSize(224, 224) << qreal(1.75);
    QTest::newRow("more than max") << QSize(300, 300) << qreal(1.75);
    QTest::newRow("insufficient") << QSize(150, 150) << (qreal(150) / 128);
    QTest::newRow("below 1x") << QSize(100, 100) << qreal(1.0);
    QTest::newRow("non-square uses longer edge") << QSize(224, 90) << qreal(1.75);
}

void FilePreviewJobTest::testSupportedDevicePixelRatio()
{
    QFETCH(QSize, imageSize);
    QFETCH(qreal, expectedDpr);

    QCOMPARE(KIO::supportedDevicePixelRatio(imageSize, QSize(128, 128), 1.75), expectedDpr);
}

namespace
{
// Creates a regular file with some content and returns a KFileItem carrying its modification time
// and size, as an item from a directory listing does. The cached thumbnail of a file is told from
// an outdated one by those two, so an item without them can answer nothing.
KFileItem makeFileItem(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("some content");
        file.close();
    }

    const QFileInfo info(path);
    KIO::UDSEntry entry;
    entry.reserve(4);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, info.fileName());
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, info.size());
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, info.lastModified().toSecsSinceEpoch());
    return KFileItem(entry, QUrl::fromLocalFile(path));
}

// Writes a thumbnail into the cache for the given source url at size and ratio, stamped with the
// freedesktop metadata a thumbnailer writes.
void writeCachedThumbnail(const QUrl &url, const QSize &size, qreal dpr, qint64 mtimeSecs, qint64 fileSize)
{
    const QByteArray uri = url.toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded);
    const QString path = ThumbnailCache::filePath(uri, ThumbnailCache::rootPath(), size, dpr);
    QDir().mkpath(QFileInfo(path).absolutePath());

    QImage thumb(64, 64, QImage::Format_ARGB32);
    thumb.fill(Qt::blue);
    thumb.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(uri));
    thumb.setText(QStringLiteral("Thumb::MTime"), QString::number(mtimeSecs));
    thumb.setText(QStringLiteral("Thumb::Size"), QString::number(fileSize));
    thumb.save(path, "png");
}
}

void FilePreviewJobTest::testCachedPreviewReturnsFreshThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));
    const QSize size(256, 256);

    writeCachedThumbnail(item.targetUrl(), size, 1.0, item.time(KFileItem::ModificationTime).toSecsSinceEpoch(), item.size());

    const QImage fresh = PreviewJob::cachedPreview(item, size, 1.0);
    QVERIFY(!fresh.isNull());
    QVERIFY(PreviewJob::cachedPreviewMatchesFile(fresh, item));
}

// A thumbnail made from an older state of the file is still handed over, to be shown while a
// fresh one is generated, and is told apart by cachedPreviewMatchesFile().
void FilePreviewJobTest::testCachedPreviewServesAStaleThumbnail()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));
    const QSize size(256, 256);

    writeCachedThumbnail(item.targetUrl(), size, 1.0, item.time(KFileItem::ModificationTime).toSecsSinceEpoch() - 60, item.size());

    const QImage stale = PreviewJob::cachedPreview(item, size, 1.0);
    QVERIFY(!stale.isNull());
    QVERIFY(!PreviewJob::cachedPreviewMatchesFile(stale, item));
}

void FilePreviewJobTest::testCachedPreviewMissReturnsNull()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));

    QVERIFY(PreviewJob::cachedPreview(item, QSize(256, 256), 1.0).isNull());
}

// A folder and a remote file have no thumbnail this synchronous path can read.
void FilePreviewJobTest::testCachedPreviewRejectsIneligibleItems()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QSize size(256, 256);

    const KFileItem dirItem(QUrl::fromLocalFile(dir.path()));
    QVERIFY(PreviewJob::cachedPreview(dirItem, size, 1.0).isNull());

    const KFileItem remoteItem(QUrl(QStringLiteral("ftp://example.com/photo.png")));
    QVERIFY(PreviewJob::cachedPreview(remoteItem, size, 1.0).isNull());
}

// The directory a thumbnail is looked for in has to be the one it is written to. What is written
// is generated at the cache size of the request times the ratio of the screen, so that product is
// what names the directory, and a size that no directory takes is not cached at all.
void FilePreviewJobTest::testCacheDirMatchesWhatIsWritten_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<qreal>("devicePixelRatio");
    QTest::addColumn<QString>("expectedDir");

    QTest::newRow("64 at 1") << QSize(64, 64) << 1.0 << QStringLiteral("normal/");
    QTest::newRow("64 at 1.25") << QSize(64, 64) << 1.25 << QStringLiteral("large/");
    QTest::newRow("64 at 1.5") << QSize(64, 64) << 1.5 << QStringLiteral("large/");
    QTest::newRow("64 at 2") << QSize(64, 64) << 2.0 << QStringLiteral("large/");
    QTest::newRow("64 at 3") << QSize(64, 64) << 3.0 << QStringLiteral("x-large/");
    QTest::newRow("128 at 1") << QSize(128, 128) << 1.0 << QStringLiteral("normal/");
    QTest::newRow("128 at 2") << QSize(128, 128) << 2.0 << QStringLiteral("large/");
    QTest::newRow("129 at 1") << QSize(129, 129) << 1.0 << QStringLiteral("large/");
    QTest::newRow("129 at 1.25") << QSize(129, 129) << 1.25 << QStringLiteral("x-large/");
    QTest::newRow("160 at 2.5") << QSize(160, 160) << 2.5 << QStringLiteral("xx-large/");
    QTest::newRow("256 at 4") << QSize(256, 256) << 4.0 << QStringLiteral("xx-large/");
    QTest::newRow("512 at 4") << QSize(512, 512) << 4.0 << QString();
    QTest::newRow("600 at 2") << QSize(600, 600) << 2.0 << QString();
}

void FilePreviewJobTest::testCacheDirMatchesWhatIsWritten()
{
    QFETCH(QSize, size);
    QFETCH(qreal, devicePixelRatio);
    QFETCH(QString, expectedDir);

    QCOMPARE(ThumbnailCache::tierDir(ThumbnailCache::cacheSize(size), devicePixelRatio), expectedDir);

    const QString path = ThumbnailCache::filePath(QByteArrayLiteral("file:///a"), QStringLiteral("/thumbs/"), size, devicePixelRatio);
    if (expectedDir.isEmpty()) {
        QVERIFY(path.isEmpty());
    } else {
        QVERIFY(path.startsWith(QStringLiteral("/thumbs/") + expectedDir));
    }
}

// A thumbnail cached for a screen of a given ratio is found again by a request of that ratio.
void FilePreviewJobTest::testCachedPreviewFoundAtDevicePixelRatio_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<qreal>("devicePixelRatio");

    QTest::newRow("64 at 1") << QSize(64, 64) << 1.0;
    QTest::newRow("64 at 1.5") << QSize(64, 64) << 1.5;
    QTest::newRow("64 at 2") << QSize(64, 64) << 2.0;
    QTest::newRow("128 at 2") << QSize(128, 128) << 2.0;
    QTest::newRow("200 at 1.25") << QSize(200, 200) << 1.25;
    QTest::newRow("256 at 2") << QSize(256, 256) << 2.0;
}

void FilePreviewJobTest::testCachedPreviewFoundAtDevicePixelRatio()
{
    QFETCH(QSize, size);
    QFETCH(qreal, devicePixelRatio);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));

    writeCachedThumbnail(item.targetUrl(), size, devicePixelRatio, item.time(KFileItem::ModificationTime).toSecsSinceEpoch(), item.size());

    QVERIFY(!PreviewJob::cachedPreview(item, size, devicePixelRatio).isNull());
}

// A thumbnail too large for any of the cache directories has none to go in, so it is made for the
// request alone: no directory is named for it, and nothing is read from the root of the cache,
// where such a thumbnail used to be written.
void FilePreviewJobTest::testNoCacheBucketLeavesTheCacheAlone()
{
    const QSize size(512, 512);
    const qreal devicePixelRatio = 4.0;

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const KFileItem item = makeFileItem(dir.filePath(QStringLiteral("photo.png")));
    const QByteArray uri = item.targetUrl().toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded);
    const QString thumbRoot = ThumbnailCache::rootPath();

    QVERIFY(ThumbnailCache::tierDir(ThumbnailCache::cacheSize(size), devicePixelRatio).isEmpty());
    QVERIFY(ThumbnailCache::filePath(uri, thumbRoot, size, devicePixelRatio).isEmpty());

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(uri);
    const QString strayPath = thumbRoot + QString::fromLatin1(md5.result().toHex()) + QLatin1String(".png");
    QDir().mkpath(thumbRoot);
    QImage stray(64, 64, QImage::Format_ARGB32);
    stray.fill(Qt::red);
    stray.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(uri));
    stray.setText(QStringLiteral("Thumb::MTime"), QString::number(item.time(KFileItem::ModificationTime).toSecsSinceEpoch()));
    stray.setText(QStringLiteral("Thumb::Size"), QString::number(item.size()));
    QVERIFY(stray.save(strayPath, "png"));

    QVERIFY(PreviewJob::cachedPreview(item, size, devicePixelRatio).isNull());

    QVERIFY(QFile::remove(strayPath));
}

/**
 * An item whose cached thumbnail is looked for and not found still has its preview made in the
 * order the caller asked for. It used to be made after every item that skipped the lookup, so the
 * items on screen, the only ones whose type is known early enough to be looked up, were made last.
 */
void FilePreviewJobTest::testAMissedCacheLookupKeepsItsPlaceInTheQueue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Empty files, so that every one of them is refused for want of content instead of having a
    // thumbnailer run over it. What is under test is the order they are dealt with in, and an
    // empty file reaches that point the same way a full one does.
    const auto emptyFile = [&dir](const QString &name) {
        const QString path = dir.filePath(name);
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.close();
        KIO::UDSEntry entry;
        entry.reserve(4);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
        entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, QFileInfo(path).lastModified().toSecsSinceEpoch());
        return KFileItem(entry, QUrl::fromLocalFile(path));
    };

    // The first item is the one that gets looked up: its type is known, and nothing is cached for
    // it. The others have no type yet, so the lookup skips them.
    KFileItemList items;
    KFileItem lookedUp = emptyFile(QStringLiteral("00-looked-up.png"));
    lookedUp.determineMimeType();
    QVERIFY(lookedUp.isMimeTypeKnown());
    items.append(lookedUp);

    // Enough of them that the position in the queue outweighs the reordering a handful of
    // concurrent thumbnail workers can cause.
    const int others = 39;
    for (int i = 0; i < others; ++i) {
        const QString name = QStringLiteral("%1-other.png").arg(i + 1, 2, 10, QLatin1Char('0'));
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        items.append(KFileItem(QUrl::fromLocalFile(dir.filePath(name))));
    }

    QStringList plugins = PreviewJob::defaultPlugins();
    auto *job = new PreviewJob(items, QSize(256, 256), &plugins);
    job->setUiDelegate(nullptr);

    // Whatever becomes of each item, made or refused, the order it was dealt with in is the order
    // the outcomes arrive in.
    QStringList outcomes;
    connect(job, &PreviewJob::generated, this, [&outcomes](const KFileItem &item, const QImage &) {
        outcomes.append(item.url().fileName());
    });
    connect(job, &PreviewJob::failed, this, [&outcomes](const KFileItem &item) {
        outcomes.append(item.url().fileName());
    });

    QSignalSpy resultSpy(job, &KJob::result);
    QVERIFY(resultSpy.wait(60000));
    QCOMPARE(outcomes.count(), items.count());

    const int place = outcomes.indexOf(QStringLiteral("00-looked-up.png"));
    QVERIFY(place >= 0);
    // It was asked for first, so it is not left until after the rest.
    QVERIFY2(place < outcomes.count() / 2,
             qPrintable(QStringLiteral("dealt with %1 of %2: %3").arg(place + 1).arg(outcomes.count()).arg(outcomes.join(QLatin1Char(' ')))));
}
