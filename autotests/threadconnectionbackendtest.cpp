/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QMutex>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

#include <threadconnectionbackend_p.h>

using namespace KIO;

// Collects Tasks emitted by a backend's commandReceived(), thread-safely (the worker side
// emits from its own thread, the application side from the main thread).
class TaskSink
{
public:
    void add(const Task &task)
    {
        QMutexLocker lock(&m_mutex);
        m_tasks.append(task);
    }
    int count()
    {
        QMutexLocker lock(&m_mutex);
        return m_tasks.size();
    }
    QList<Task> tasks()
    {
        QMutexLocker lock(&m_mutex);
        return m_tasks;
    }

private:
    QMutex m_mutex;
    QList<Task> m_tasks;
};

class ThreadConnectionBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testApplicationReceivesFromWorker();
    void testWorkerReceivesFromApplication();
    void testOwnedPayloadSurvivesSenderScope();
    void testManyTasksPreserveOrderUnderBackPressure();
    void testWorkerCloseDisconnectsApplication();
};

void ThreadConnectionBackendTest::testApplicationReceivesFromWorker()
{
    auto [appBackend, workerBackend] = ThreadConnectionBackend::createPair();

    TaskSink sink;
    connect(appBackend.get(), &ConnectionBackend::commandReceived, this, [&sink](const Task &task) {
        sink.add(task);
    });

    // sendCommand only touches the mutex-protected channel and posts a queued wakeup, so it
    // is safe to call from any thread. Here the "worker" produces from the main thread.
    QVERIFY(workerBackend->sendCommand(1, QByteArrayLiteral("a")));
    QVERIFY(workerBackend->sendCommand(2, QByteArrayLiteral("bb")));
    QVERIFY(workerBackend->sendCommand(3, QByteArrayLiteral("ccc")));

    QTRY_COMPARE(sink.count(), 3); // delivered via the application event loop
    const QList<Task> tasks = sink.tasks();
    QCOMPARE(tasks.at(0).cmd, 1);
    QCOMPARE(tasks.at(0).data, QByteArrayLiteral("a"));
    QCOMPARE(tasks.at(2).cmd, 3);
    QCOMPARE(tasks.at(2).data, QByteArrayLiteral("ccc"));
}

void ThreadConnectionBackendTest::testWorkerReceivesFromApplication()
{
    auto [appBackend, workerBackend] = ThreadConnectionBackend::createPair();

    TaskSink sink;
    // The worker side has no event loop: commandReceived() is emitted inline from
    // waitForIncomingTask(), on the worker thread. Connect without a context so it stays
    // a direct connection delivered on that thread.
    connect(workerBackend.get(), &ConnectionBackend::commandReceived, [&sink](const Task &task) {
        sink.add(task);
    });

    QThread *workerThread = QThread::create([worker = workerBackend.get()] {
        // Poll like SlaveBase::dispatchLoop() does, until the channel is closed.
        while (worker->state == ConnectionBackend::Connected) {
            worker->waitForIncomingTask(200);
        }
    });
    workerThread->start();

    QVERIFY(appBackend->sendCommand(10, QByteArrayLiteral("x")));
    QVERIFY(appBackend->sendCommand(20, QByteArrayLiteral("yy")));

    QTRY_COMPARE(sink.count(), 2);

    appBackend->closeSocket(); // makes waitForIncomingTask() return and the worker loop exit
    QVERIFY(workerThread->wait(5000));
    delete workerThread;

    const QList<Task> tasks = sink.tasks();
    QCOMPARE(tasks.at(0).cmd, 10);
    QCOMPARE(tasks.at(1).data, QByteArrayLiteral("yy"));
}

void ThreadConnectionBackendTest::testOwnedPayloadSurvivesSenderScope()
{
    // The backend shares the payload rather than copying it (callers must pass an owned
    // QByteArray). Its refcount must keep the bytes alive until the application consumes
    // the task, even after the sender drops its own reference.
    auto [appBackend, workerBackend] = ThreadConnectionBackend::createPair();

    TaskSink sink;
    connect(appBackend.get(), &ConnectionBackend::commandReceived, this, [&sink](const Task &task) {
        sink.add(task);
    });

    {
        const QByteArray owned(64, 'z'); // owns its bytes
        QVERIFY(workerBackend->sendCommand(7, owned));
    } // owned goes out of scope here, the in-flight task must keep the bytes alive

    QTRY_COMPARE(sink.count(), 1);
    QCOMPARE(sink.tasks().at(0).cmd, 7);
    QCOMPARE(sink.tasks().at(0).data, QByteArray(64, 'z'));
}

void ThreadConnectionBackendTest::testManyTasksPreserveOrderUnderBackPressure()
{
    auto [appBackend, workerBackend] = ThreadConnectionBackend::createPair();

    TaskSink sink;
    connect(appBackend.get(), &ConnectionBackend::commandReceived, this, [&sink](const Task &task) {
        sink.add(task);
    });

    // Produce far more than HighWaterMark from a separate thread so the producer actually
    // blocks on back-pressure and resumes as the application drains. Order must be preserved.
    const int total = 2000;
    QThread *workerThread = QThread::create([worker = workerBackend.get(), total] {
        for (int i = 0; i < total; ++i) {
            worker->sendCommand(i, QByteArray::number(i));
        }
    });
    workerThread->start();

    QTRY_COMPARE_WITH_TIMEOUT(sink.count(), total, 30000);
    QVERIFY(workerThread->wait(5000));
    delete workerThread;

    const QList<Task> tasks = sink.tasks();
    for (int i = 0; i < total; ++i) {
        QCOMPARE(tasks.at(i).cmd, i);
        QCOMPARE(tasks.at(i).data, QByteArray::number(i));
    }
}

void ThreadConnectionBackendTest::testWorkerCloseDisconnectsApplication()
{
    auto [appBackend, workerBackend] = ThreadConnectionBackend::createPair();

    QSignalSpy disconnectedSpy(appBackend.get(), &ConnectionBackend::disconnected);
    QVERIFY(disconnectedSpy.isValid());

    workerBackend->closeSocket(); // worker end goes away -> application is notified

    QTRY_COMPARE(disconnectedSpy.count(), 1);
    QCOMPARE(appBackend->state, ConnectionBackend::Idle);
}

QTEST_MAIN(ThreadConnectionBackendTest)

#include "threadconnectionbackendtest.moc"
