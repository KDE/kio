/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// A minimal real KIO worker for the "kiotest" protocol, used by the autotests to exercise the
// out-of-process (socket) worker path - the in-process mock factory (setTestWorkerFactory) can only
// test the threaded path. Behaviour is keyed off the request URL host so a single worker covers the
// test cases.

#include <QCoreApplication>
#include <QThread>
#include <QUrl>

#include <cstdlib>
#include <sys/stat.h>

#include "workerbase.h"
#include "workerfactory.h"

using namespace KIO;

namespace
{
class TestWorker : public WorkerBase
{
public:
    TestWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase(QByteArrayLiteral("kiotest"), pool, app)
    {
    }

    WorkerResult stat(const QUrl &url) override
    {
        if (url.host() == QLatin1String("fail")) {
            return WorkerResult::fail(ERR_ACCESS_DENIED, QStringLiteral("mock denied"));
        }
        UDSEntry entry;
        const QString name = url.fileName().isEmpty() ? QStringLiteral("/") : url.fileName();
        entry.fastInsert(UDSEntry::UDS_NAME, name);
        entry.fastInsert(UDSEntry::UDS_FILE_TYPE, S_IFREG);
        entry.fastInsert(UDSEntry::UDS_SIZE, 5);
        statEntry(entry);
        return WorkerResult::pass();
    }

    WorkerResult listDir(const QUrl &url) override
    {
        // host "withdot": the worker lists "." itself (SlaveBase must not synthesize a duplicate).
        // Otherwise it lists many plain files and no "." (SlaveBase must synthesize one in finished()),
        // enough to span more than one MSG_LIST_ENTRIES batch.
        const bool withDot = url.host() == QLatin1String("withdot");
        if (withDot) {
            UDSEntry dot;
            dot.fastInsert(UDSEntry::UDS_NAME, QStringLiteral("."));
            dot.fastInsert(UDSEntry::UDS_FILE_TYPE, S_IFDIR);
            listEntry(dot);
        }
        const int count = withDot ? 3 : 250;
        for (int i = 0; i < count; ++i) {
            UDSEntry entry;
            entry.fastInsert(UDSEntry::UDS_NAME, QStringLiteral("file%1").arg(i));
            entry.fastInsert(UDSEntry::UDS_FILE_TYPE, S_IFREG);
            listEntry(entry);
        }
        return WorkerResult::pass();
    }

    WorkerResult get(const QUrl &url) override
    {
        if (url.host() == QLatin1String("die")) {
            // Simulate a worker process crashing mid-job: the application must turn the lost
            // connection into a clean job error, not hang.
            std::_Exit(1);
        }
        mimeType(QStringLiteral("text/plain"));
        if (url.host() == QLatin1String("slow")) {
            // Stream slowly so the test can put us on hold mid-transfer before we finish.
            for (int i = 0; i < 40; ++i) {
                data(QByteArray("chunk"));
                QThread::msleep(25);
            }
        } else {
            data(QByteArray("hello"));
        }
        data(QByteArray()); // EOF
        return WorkerResult::pass();
    }
};

class KIOPluginFactory : public WorkerFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.kiotest" FILE "kiotestworker.json")

public:
    std::unique_ptr<WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
    {
        return std::make_unique<TestWorker>(pool, app);
    }
};
}

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_test"));

    if (argc != 4) {
        fprintf(stderr, "Usage: kio_test protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }

    TestWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

#include "kiotestworker.moc"
