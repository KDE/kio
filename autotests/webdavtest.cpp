/*
    SPDX-FileCopyrightText: 2019 Ben Gruber <bengruber250@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

// This test suite is based on those in ftptest.cpp and uses the same test files.

#include <kio/copyjob.h>
#include <kio/job.h>

#include <QBuffer>
#include <QProcess>
#include <QStandardPaths>
#include <QTest>

class WebDAVTest : public QObject
{
    Q_OBJECT
public:
    QUrl url(const QString &path) const
    {
        Q_ASSERT(path.startsWith(QChar('/')));
        QUrl newUrl = m_url;
        newUrl.setPath(path);
        newUrl.setPort(port);
        return newUrl;
    }

    QTemporaryDir m_remoteDir;
    QProcess m_daemonProc;
    QUrl m_url = QUrl("webdav://localhost");
    static const int port = 30000;

private:
    static void runDaemon(QProcess &proc, const QTemporaryDir &remoteDir)
    {
        QVERIFY(remoteDir.isValid());
        proc.setProgram("wsgidav");
        proc.setArguments({ QStringLiteral("--host=0.0.0.0"), QString("--port=%1").arg(port),
            QString("--root=%1").arg(remoteDir.path()),
            QStringLiteral("--auth=anonymous") });
        proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        proc.start();
        QVERIFY(proc.waitForStarted());
        QCOMPARE(proc.state(), QProcess::Running);
        // Wait for the daemon to print its port. That tells us both where it's listening
        // and also that it is ready to move ahead with testing.
        QVERIFY(QTest::qWaitFor([&]() -> bool {
            const QString out = proc.readAllStandardOutput();
            if (!out.isEmpty()) {
                qDebug() << "STDERR:" << out;
            }
            if (!out.endsWith("Serving on http://0.0.0.0:30000 ...\n")) {
                return false;
            }
            return true;
        }, 5000));
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Force the http/webdav slave from our bindir as first choice. This specifically
        // works around the fact that kioslave would load the slave from the system
        // as first choice instead of the one from the build dir.
        qputenv("QT_PLUGIN_PATH", QCoreApplication::applicationDirPath().toUtf8());

        // Start the webdav server.
        runDaemon(m_daemonProc, m_remoteDir);
        // Put a prefix on the stderr and stdout from the server.
        connect(&m_daemonProc, &QProcess::readyReadStandardError,
                this, [this] {
            qDebug() << "wsgidav STDERR:" << m_daemonProc.readAllStandardError();
        });
        connect(&m_daemonProc, &QProcess::readyReadStandardOutput,
                this, [this] {
            qDebug() << "wsgidav STDOUT:" << m_daemonProc.readAllStandardOutput();
        });

        QStandardPaths::setTestModeEnabled(true);
        qputenv("KDE_FORK_SLAVES", "yes");
    }

    void cleanupTestCase()
    {
        m_daemonProc.terminate();
        m_daemonProc.kill();
        m_daemonProc.waitForFinished();
    }

    void init()
    {
        QCOMPARE(m_daemonProc.state(), QProcess::Running);
    }

    void testGet()
    {
        const QString path("/testGet");
        const auto url = this->url(path);
        const QString remotePath = m_remoteDir.path() + path;

        QByteArray data("testBasicGet");
        QFile file(remotePath);
        QVERIFY(file.open(QFile::WriteOnly));
        file.write(data);
        file.close();

        auto job = KIO::storedGet(url);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->data(), data);
    }

    void testCopy()
    {
        const QString path("/testCopy");
        const auto url = this->url(path);
        const QString remotePath = m_remoteDir.path() + path;
        const QString partPath = remotePath + ".part";

        QFile::remove(remotePath);
        QFile::remove(partPath);

        const QString testCopy1 = QFINDTESTDATA("ftp/testCopy1");
        QVERIFY(!testCopy1.isEmpty());
        auto job = KIO::copy({ QUrl::fromLocalFile(testCopy1) }, url, KIO::DefaultFlags);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));
        QFile file(remotePath);
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("part1\n"));
    }

    void testCopyResume()
    {
        const QString path("/testCopy");
        const auto url = this->url(path);
        const QString remotePath = m_remoteDir.path() + path;
        const QString partPath = remotePath + ".part";

        QFile::remove(remotePath);
        QFile::remove(partPath);
        const QString testCopy1 = QFINDTESTDATA("ftp/testCopy1");
        QVERIFY(!testCopy1.isEmpty());
        QVERIFY(QFile::copy(testCopy1,
                            partPath));

        const QString testCopy2 = QFINDTESTDATA("ftp/testCopy2");
        QVERIFY(!testCopy2.isEmpty());
        auto job = KIO::copy({ QUrl::fromLocalFile(testCopy2) }, url, KIO::Resume);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));
        QFile file(remotePath);
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("part1\npart2\n"));
    }

    void testOverwriteCopy()
    {
        const QString path("/testOverwriteCopy");
        const auto url = this->url(path);
        const QString remotePath = m_remoteDir.path() + path;

        qDebug() << (m_remoteDir.path() + path);

        // Create file
        const QString testCopy1 = QFINDTESTDATA("ftp/testCopy1");
        QVERIFY(!testCopy1.isEmpty());
        auto job1 = KIO::copy({ QUrl::fromLocalFile(testCopy1) }, url, KIO::DefaultFlags);
        job1->setUiDelegate(nullptr);
        QVERIFY2(job1->exec(), qUtf8Printable(job1->errorString()));
        QFile file(remotePath);
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("part1\n"));
        file.close();

        // File already exists, we expect it to be overwritten.
        const QString testOverwriteCopy2 = QFINDTESTDATA("ftp/testOverwriteCopy2");
        QVERIFY(!testOverwriteCopy2.isEmpty());
        auto job2 = KIO::copy({ QUrl::fromLocalFile(testOverwriteCopy2) }, url, KIO::Overwrite);
        job2->setUiDelegate(nullptr);
        QVERIFY2(job2->exec(), qUtf8Printable(job2->errorString()));
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("testOverwriteCopy2\n"));
    }

    void testOverwriteCopyWithoutFlagFromLocal()
    {
        const QString path("/testOverwriteCopyWithoutFlag");
        const auto url = this->url(path);

        qDebug() << (m_remoteDir.path() + path);
        const QString testOverwriteCopy1 = QFINDTESTDATA("ftp/testOverwriteCopy1");
        QVERIFY(!testOverwriteCopy1.isEmpty());
        QVERIFY(QFile::copy(testOverwriteCopy1,
                            m_remoteDir.path() + path));

        // Without overwrite flag.
        const QString testOverwriteCopy2 = QFINDTESTDATA("ftp/testOverwriteCopy2");
        QVERIFY(!testOverwriteCopy2.isEmpty());
        auto job = KIO::copy({ QUrl::fromLocalFile(testOverwriteCopy2) }, url, KIO::DefaultFlags);
        job->setUiDelegate(nullptr);
        QVERIFY2(!job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->error(), KIO::ERR_FILE_ALREADY_EXIST);
        QFile file(m_remoteDir.path() + path);
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("testOverwriteCopy1\n")); // not 2!
    }

    void testOverwriteCopyWithoutFlagFromRemote()
    {
        // This excercises a different code path than testOverwriteCopyWithoutFlagFromLocal
        const QString path("/testOverwriteCopyWithoutFlagRemote");
        const QString dir_path("/dir");
        const auto url = this->url(path);
        const auto dir_url = this->url(dir_path);

        qDebug() << (m_remoteDir.path() + path);
        const auto testOverwriteCopy1 = QFINDTESTDATA("ftp/testOverwriteCopy1");
        QVERIFY(!testOverwriteCopy1.isEmpty());
        QVERIFY(QFile::copy(testOverwriteCopy1,
                            m_remoteDir.path() + path));
        QVERIFY(QDir(m_remoteDir.path()).mkdir("dir"));

        // First copy should work.
        auto job = KIO::copy(url, dir_url, KIO::DefaultFlags);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));

        // Without overwrite flag.
        auto job2 = KIO::copy(url, dir_url, KIO::DefaultFlags);
        job2->setUiDelegate(nullptr);
        QVERIFY2(!job2->exec(), qUtf8Printable(job2->errorString()));
        QCOMPARE(job2->error(), KIO::ERR_FILE_ALREADY_EXIST);
        QFile file(m_remoteDir.path() + path);
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("testOverwriteCopy1\n")); // not 2!
    }
};

QTEST_MAIN(WebDAVTest)
#include "webdavtest.moc"
