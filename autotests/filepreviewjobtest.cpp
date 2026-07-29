/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Meven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filepreviewjobtest.h"

#include "../src/gui/filepreviewjob.h"

#include <KFileItem>

#include <QDateTime>
#include <QImage>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>

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
