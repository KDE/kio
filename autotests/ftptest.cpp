/*
    SPDX-FileCopyrightText: 2019 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <kio/copyjob.h>
#include <kio/job.h>

#include <QBuffer>
#include <QProcess>
#include <QStandardPaths>
#include <QTest>

class FTPTest : public QObject
{
    Q_OBJECT
public:
    QUrl url(const QString &path) const
    {
        Q_ASSERT(path.startsWith(QChar('/')));
        QUrl newUrl = m_url;
        newUrl.setPath(path);
        return newUrl;
    }

    QTemporaryDir m_remoteDir;
    QProcess m_daemonProc;
    QUrl m_url = QUrl("ftp://localhost");

private Q_SLOTS:
    static void runDaemon(QProcess &proc, QUrl &url, const QTemporaryDir &remoteDir)
    {
        QVERIFY(remoteDir.isValid());
        proc.setProgram(RubyExe_EXECUTABLE);
        proc.setArguments({ QFINDTESTDATA("ftpd"), QStringLiteral("0"), remoteDir.path() });
        proc.setProcessChannelMode(QProcess::ForwardedOutputChannel);
        qDebug() << proc.arguments();
        proc.start();
        QVERIFY(proc.waitForStarted());
        QCOMPARE(proc.state(), QProcess::Running);
        // Wait for the daemon to print its port. That tells us both where it's listening
        // and also that it is ready to move ahead with testing.
        QVERIFY(QTest::qWaitFor([&]() -> bool {
            const QString err = proc.readAllStandardError();
            if (!err.isEmpty()) {
                qDebug() << "STDERR:" << err;
            }
            if (!err.startsWith("port = ")) {
                return false;
            }
            bool ok = false;
            const int port = err.split(" = ").at(1).toInt(&ok);
            url.setPort(port);
            return ok;
        }, 8000));
    }

    void initTestCase()
    {
        // Force the ftp slave from our bindir as first choice. This specifically
        // works around the fact that kioslave would load the slave from the system
        // as first choice instead of the one from the build dir.
        qputenv("QT_PLUGIN_PATH", QCoreApplication::applicationDirPath().toUtf8());

        // Run ftpd to talk to.
        runDaemon(m_daemonProc, m_url, m_remoteDir);
        // Once it's started we can simply forward the output. Possibly should do the
        // same for stdout so it has a prefix.
        connect(&m_daemonProc, &QProcess::readyReadStandardError,
                this, [this] {
            qDebug() << "ftpd STDERR:" << m_daemonProc.readAllStandardError();
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

        auto job = KIO::copy({ QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy1")) }, url, KIO::DefaultFlags);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->error(), 0);
        QFile file(remotePath);
        QVERIFY(file.exists());
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
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testCopy1"),
                            partPath));

        auto job = KIO::copy({ QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy2")) }, url, KIO::Resume);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->error(), 0);
        QFile file(remotePath);
        QVERIFY(file.exists());
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("part1\npart2\n"));
    }

    void testCopyInaccessible()
    {
        const QString inaccessiblePath("/testCopy.__inaccessiblePath__");
        auto inaccessibleUrl = this->url(inaccessiblePath);

        auto job = KIO::copy({ QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy1")) }, inaccessibleUrl, KIO::Resume);
        job->setUiDelegate(nullptr);
        QVERIFY(!job->exec());
        QCOMPARE(job->error(), KIO::ERR_CANNOT_WRITE);
        QFile file(inaccessiblePath);
        QVERIFY(!file.exists());
    }

    void testCopyBadResume()
    {
        const QString inaccessiblePath("/testCopy.__badResume__");
        auto inaccessibleUrl = this->url(inaccessiblePath);
        inaccessibleUrl.setUserInfo("user");
        inaccessibleUrl.setPassword("password");
        const QString remoteInaccessiblePath = m_remoteDir.path() + inaccessiblePath;
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testCopy1"),
                            remoteInaccessiblePath + ".part"));

        auto job = KIO::copy({ QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy2")) }, inaccessibleUrl, KIO::Resume);
        job->setUiDelegate(nullptr);
        QVERIFY(!job->exec());
        QCOMPARE(job->error(), KIO::ERR_CANNOT_WRITE);
        QFile file(inaccessiblePath);
        QVERIFY(!file.exists());
    }

    void testOverwriteCopy()
    {
        const QString path("/testOverwriteCopy");
        const auto url = this->url(path);

        qDebug() << (m_remoteDir.path() + path);
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testOverwriteCopy1"),
                            m_remoteDir.path() + path));

        // File already exists, we expect it to be overwritten.
        auto job = KIO::copy({ QUrl::fromLocalFile(QFINDTESTDATA("ftp/testOverwriteCopy2")) }, url, KIO::Overwrite);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->error(), 0);
        QFile file(m_remoteDir.path() + path);
        QVERIFY(file.exists());
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("testOverwriteCopy2\n"));
    }

    void testOverwriteCopyWithoutFlag()
    {
        const QString path("/testOverwriteCopyWithoutFlag");
        const auto url = this->url(path);

        qDebug() << (m_remoteDir.path() + path);
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testOverwriteCopy1"),
                            m_remoteDir.path() + path));

        // Without overwrite flag.
        // https://bugs.kde.org/show_bug.cgi?id=409954
        auto job = KIO::copy({ QUrl::fromLocalFile(QFINDTESTDATA("ftp/testOverwriteCopy2")) }, url, KIO::DefaultFlags);
        job->setUiDelegate(nullptr);
        QVERIFY2(!job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->error(), KIO::ERR_FILE_ALREADY_EXIST);
        QFile file(m_remoteDir.path() + path);
        QVERIFY(file.exists());
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("testOverwriteCopy1\n")); // not 2!
    }
};

QTEST_MAIN(FTPTest)
#include "ftptest.moc"
