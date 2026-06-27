/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QByteArray>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTest>

#include "kio/storedtransferjob.h"

// Exercises the out-of-process worker transport: a worker launched as a separate kioworker process
// and talked to over a socket, as opposed to the in-process thread workers other tests use.
class SocketWorkerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Force the socket transport. By default kio_file runs in-process over a thread; with this
        // disabled it is launched as a kioworker process and reached over a socket. bUseThreads in
        // Worker::createWorker() is read at the first worker creation, which happens after this, so
        // every worker in this test uses the socket transport.
        qputenv("KIO_ENABLE_WORKER_THREADS", "0");
    }

    void readsFileOverSocketWorker()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        const QByteArray content = "socket worker payload";
        QCOMPARE(file.write(content), content.size());
        file.flush();

        // A get() served by an out-of-process kio_file worker returns the file's contents.
        auto *job = KIO::storedGet(QUrl::fromLocalFile(file.fileName()), KIO::NoReload, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(job->data(), content);
    }
};

QTEST_MAIN(SocketWorkerTest)

#include "socketworkertest.moc"
