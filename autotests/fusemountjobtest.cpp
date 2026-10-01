/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "fusemountjob.h"

#include <kio/global.h>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QSignalSpy>
#include <QTest>

#include <utility>

// Stands in for kio-fuse. It lives on a connection of its own, so that the job's call
// really goes through the bus and the reply can be held back.
class MockKioFuse : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KIOFuse.VFS")

public:
    MockKioFuse()
        : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("mock-kiofuse")))
    {
    }

    ~MockKioFuse() override
    {
        unregisterService();
        QDBusConnection::disconnectFromBus(QStringLiteral("mock-kiofuse"));
    }

    bool registerService()
    {
        return m_bus.registerObject(QStringLiteral("/org/kde/KIOFuse"), this, QDBusConnection::ExportAllSlots)
            && m_bus.registerService(QStringLiteral("org.kde.KIOFuse"));
    }

    void unregisterService()
    {
        m_bus.unregisterObject(QStringLiteral("/org/kde/KIOFuse"));
        m_bus.unregisterService(QStringLiteral("org.kde.KIOFuse"));
    }

    void releasePending()
    {
        const QList<QDBusMessage> calls = std::exchange(pending, {});
        for (const QDBusMessage &call : calls) {
            m_bus.send(replyTo(call));
        }
    }

    bool hold = false;
    QString errorName;
    QStringList requested;
    QList<QDBusMessage> pending;

public Q_SLOTS:
    QString mountUrl(const QString &remoteUrl, const QDBusMessage &message)
    {
        message.setDelayedReply(true);
        requested << remoteUrl;
        if (hold) {
            pending << message;
        } else {
            m_bus.send(replyTo(message));
        }
        return QString();
    }

private:
    QDBusMessage replyTo(const QDBusMessage &message) const
    {
        if (!errorName.isEmpty()) {
            return message.createErrorReply(errorName, QStringLiteral("mock failure"));
        }
        const QUrl url(message.arguments().constFirst().toString());
        const QString localPath = QStringLiteral("/mock-kio-fuse/") + url.scheme() + QLatin1Char('/') + url.authority() + url.path();
        return message.createReply(localPath);
    }

    QDBusConnection m_bus;
};

class FuseMountJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void testLocalFile();
    void testMalformedUrl();
    void testMount();
    void testMountError_data();
    void testMountError();
    void testTimeout();
    void testKillWhileWaiting();
    void testServiceUnavailable();

private:
    MockKioFuse *m_mock = nullptr;
};

void FuseMountJobTest::initTestCase()
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        QSKIP("No session bus");
    }
    m_mock = new MockKioFuse;
    if (!m_mock->registerService()) {
        QSKIP("org.kde.KIOFuse is already taken on this bus");
    }
}

void FuseMountJobTest::init()
{
    m_mock->hold = false;
    m_mock->errorName.clear();
    m_mock->requested.clear();
    m_mock->pending.clear();
}

void FuseMountJobTest::cleanupTestCase()
{
    delete m_mock;
}

void FuseMountJobTest::testLocalFile()
{
    auto *job = new KIO::FuseMountJob(QUrl::fromLocalFile(QStringLiteral("/tmp/some file")));
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->localPath(), QStringLiteral("/tmp/some file"));
    QVERIFY(m_mock->requested.isEmpty());
}

void FuseMountJobTest::testMalformedUrl()
{
    auto *job = new KIO::FuseMountJob(QUrl(QStringLiteral("no-scheme")));
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_MALFORMED_URL);
    QVERIFY(m_mock->requested.isEmpty());
}

void FuseMountJobTest::testMount()
{
    const QUrl url(QStringLiteral("smb://user@server/share/dir with space"));
    auto *job = new KIO::FuseMountJob(url);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(m_mock->requested, QStringList{url.toString(QUrl::FullyEncoded)});
    QCOMPARE(job->url(), url);
    QCOMPARE(job->localPath(), QStringLiteral("/mock-kio-fuse/smb/user@server/share/dir with space"));
}

void FuseMountJobTest::testMountError_data()
{
    QTest::addColumn<QString>("errorName");
    QTest::addColumn<int>("expectedError");

    QTest::newRow("cannot-mount") << QStringLiteral("org.kde.KIOFuse.VFS.Error.CannotMount") << int(KIO::ERR_CANNOT_MOUNT);
    QTest::newRow("scheme-not-supported") << QStringLiteral("org.kde.KIOFuse.VFS.Error.SchemeNotSupported") << int(KIO::ERR_UNSUPPORTED_PROTOCOL);
}

void FuseMountJobTest::testMountError()
{
    QFETCH(QString, errorName);
    QFETCH(int, expectedError);

    m_mock->errorName = errorName;
    auto *job = new KIO::FuseMountJob(QUrl(QStringLiteral("smb://server/share")));
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), expectedError);
    QVERIFY(job->localPath().isEmpty());
    QVERIFY(!job->errorString().isEmpty());
}

void FuseMountJobTest::testTimeout()
{
    m_mock->hold = true;
    auto *job = new KIO::FuseMountJob(QUrl(QStringLiteral("smb://server/share")));
    job->setTimeout(100);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_SERVER_TIMEOUT);
}

void FuseMountJobTest::testKillWhileWaiting()
{
    m_mock->hold = true;
    auto *job = new KIO::FuseMountJob(QUrl(QStringLiteral("smb://server/share")));
    QSignalSpy resultSpy(job, &KJob::result);
    QSignalSpy destroyedSpy(job, &QObject::destroyed);
    job->start();
    QTRY_COMPARE(m_mock->pending.size(), 1);

    QVERIFY(job->kill());
    QVERIFY(destroyedSpy.wait());

    // The answer arriving after the job is gone must not reach it
    m_mock->releasePending();
    QTest::qWait(100);
    QCOMPARE(resultSpy.count(), 0);
}

void FuseMountJobTest::testServiceUnavailable()
{
    const QDBusReply<QStringList> activatable = QDBusConnection::sessionBus().interface()->activatableServiceNames();
    if (activatable.isValid() && activatable.value().contains(QStringLiteral("org.kde.KIOFuse"))) {
        QSKIP("kio-fuse is installed, so the bus would start it");
    }

    m_mock->unregisterService();
    auto *job = new KIO::FuseMountJob(QUrl(QStringLiteral("smb://server/share")));
    const bool ok = job->exec();
    QVERIFY(m_mock->registerService());

    QVERIFY(!ok);
    QCOMPARE(job->error(), KIO::ERR_UNSUPPORTED_ACTION);
    QVERIFY(!job->errorString().isEmpty());
}

QTEST_GUILESS_MAIN(FuseMountJobTest)

#include "fusemountjobtest.moc"
