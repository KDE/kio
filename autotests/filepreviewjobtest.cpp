/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Meven Car <meven.car@collabora.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filepreviewjobtest.h"

#include "../src/gui/filepreviewjob.h"

#include <KFileItem>

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>

QTEST_GUILESS_MAIN(FilePreviewJobTest)

using namespace KIO;

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
