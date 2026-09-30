/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

    Manual benchmark (not run by ctest): compares worker -> application throughput of the
    socket-based ConnectionBackend against the in-process ThreadConnectionBackend.
*/

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>

#include <socketconnectionbackend_p.h>
#include <threadconnectionbackend_p.h>

#include <algorithm>
#include <cstdio>

using namespace KIO;

// Delivers `count` messages of `size` bytes from a worker thread to the application thread
// over a ThreadConnectionBackend pair, returns the elapsed milliseconds.
static double runThread(int count, int size)
{
    auto [appBackend, workerBackend] = ThreadConnectionBackend::createPair();

    QEventLoop loop;
    int received = 0;
    QObject::connect(appBackend.get(), &ConnectionBackend::commandReceived, &loop, [&](const Task &) {
        if (++received == count) {
            loop.quit();
        }
    });

    QThread *thread = QThread::create([worker = workerBackend.get(), count, size] {
        for (int i = 0; i < count; ++i) {
            // Fresh owned buffer per send, like the file worker reads a fresh chunk - so the
            // zero-copy path still pays for allocating the data it moves (no reused buffer).
            QByteArray payload(size, 'x');
            worker->sendCommand(1, payload);
        }
    });

    QElapsedTimer timer;
    timer.start();
    thread->start();
    loop.exec();
    const double ms = timer.nsecsElapsed() / 1e6;

    thread->wait();
    delete thread;
    return ms;
}

// Same, but over a real QLocalSocket connection (the application listens, the worker connects
// and sends from its own thread, like an out-of-process worker would).
static double runSocket(int count, int size)
{
    SocketConnectionBackend listener;
    listener.listenForRemote();
    const QUrl address = listener.address;

    SocketConnectionBackend *appBackend = nullptr;
    QObject::connect(&listener, &SocketConnectionBackend::newConnection, &listener, [&] {
        if (!appBackend) {
            appBackend = static_cast<SocketConnectionBackend *>(listener.nextPendingConnection());
        }
    });

    auto *client = new SocketConnectionBackend();
    client->connectToRemote(address);
    while (!appBackend) {
        QCoreApplication::processEvents();
    }

    // Move the client socket to a worker thread that runs an event loop, mirroring a worker
    // that blocks in sendCommand()'s waitForBytesWritten() off the application thread.
    QThread worker;
    worker.start();
    client->moveToThread(&worker);

    QEventLoop loop;
    int received = 0;
    QObject::connect(appBackend, &ConnectionBackend::commandReceived, &loop, [&](const Task &) {
        if (++received == count) {
            loop.quit();
        }
    });

    QElapsedTimer timer;
    timer.start();
    QMetaObject::invokeMethod(
        client,
        [client, count, size] {
            for (int i = 0; i < count; ++i) {
                QByteArray payload(size, 'x');
                client->sendCommand(1, payload);
            }
        },
        Qt::QueuedConnection);
    loop.exec();
    const double ms = timer.nsecsElapsed() / 1e6;

    worker.quit();
    worker.wait();
    delete client;
    delete appBackend;
    return ms;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const qint64 totalBytes = qint64(256) * 1024 * 1024; // target volume per size
    const int maxCount = 1000000; // cap message count so the tiny sizes do not dominate the run
    const int sizes[] = {32, 128, 256, 4096, 65536, 512 * 1024};

    std::printf("%-12s %12s %12s %10s %10s %8s\n", "msg size", "socket ms", "thread ms", "sock MB/s", "thr MB/s", "speedup");
    std::printf("------------------------------------------------------------------------\n");
    for (int size : sizes) {
        const int count = int(std::min<qint64>(totalBytes / size, maxCount));
        const double mb = double(qint64(count) * size) / (1024.0 * 1024.0);

        const double sockMs = runSocket(count, size);
        const double thrMs = runThread(count, size);

        std::printf("%-12d %12.1f %12.1f %10.0f %10.0f %7.2fx\n", size, sockMs, thrMs, mb / (sockMs / 1000.0), mb / (thrMs / 1000.0), sockMs / thrMs);
    }
    return 0;
}
