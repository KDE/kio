/*
    SPDX-FileCopyrightText: 2019 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <kio/copyjob.h>
#include <kio/listjob.h>
#include <kio/statjob.h>
#include <kio/storedtransferjob.h>

#include <QBuffer>
#include <QDir>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

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
    static void runDaemon(QProcess &proc, QUrl &url, const QTemporaryDir &remoteDir, const QString &listingMode = QStringLiteral("list"))
    {
        QVERIFY(remoteDir.isValid());
        proc.setProgram(RubyExe_EXECUTABLE);
        proc.setArguments({QFINDTESTDATA("ftpd"), QStringLiteral("0"), remoteDir.path(), listingMode});
        proc.setProcessChannelMode(QProcess::ForwardedOutputChannel);
        qDebug() << proc.arguments();
        proc.start();
        QVERIFY(proc.waitForStarted());
        QCOMPARE(proc.state(), QProcess::Running);
        // Wait for the daemon to print its port. That tells us both where it's listening
        // and also that it is ready to move ahead with testing.
        QVERIFY(QTest::qWaitFor(
            [&]() -> bool {
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
            },
            8000));
    }

    void initTestCase()
    {
        // Force the ftp worker from our bindir as first choice. This specifically
        // works around the fact that the kioworker executable would load the worker from the system
        // as first choice instead of the one from the build dir.
        qputenv("QT_PLUGIN_PATH", QCoreApplication::applicationDirPath().toUtf8());

        // Run ftpd to talk to.
        runDaemon(m_daemonProc, m_url, m_remoteDir);
        // Once it's started we can simply forward the output. Possibly should do the
        // same for stdout so it has a prefix.
        connect(&m_daemonProc, &QProcess::readyReadStandardError, this, [this] {
            qDebug() << "ftpd STDERR:" << m_daemonProc.readAllStandardError();
        });

        QStandardPaths::setTestModeEnabled(true);
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

        auto job = KIO::copy({QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy1"))}, url, KIO::DefaultFlags);
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
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testCopy1"), partPath));

        auto job = KIO::copy({QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy2"))}, url, KIO::Resume);
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

        auto job = KIO::copy({QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy1"))}, inaccessibleUrl, KIO::Resume);
        job->setUiDelegate(nullptr);
        QVERIFY(!job->exec());
        QCOMPARE(job->error(), KIO::ERR_WRITE_ACCESS_DENIED);
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
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testCopy1"), remoteInaccessiblePath + ".part"));

        auto job = KIO::copy({QUrl::fromLocalFile(QFINDTESTDATA("ftp/testCopy2"))}, inaccessibleUrl, KIO::Resume);
        job->setUiDelegate(nullptr);
        QVERIFY(!job->exec());
        QCOMPARE(job->error(), KIO::ERR_WRITE_ACCESS_DENIED);
        QFile file(inaccessiblePath);
        QVERIFY(!file.exists());
    }

    void testOverwriteCopy()
    {
        const QString path("/testOverwriteCopy");
        const auto url = this->url(path);

        qDebug() << (m_remoteDir.path() + path);
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testOverwriteCopy1"), m_remoteDir.path() + path));

        // File already exists, we expect it to be overwritten.
        auto job = KIO::copy({QUrl::fromLocalFile(QFINDTESTDATA("ftp/testOverwriteCopy2"))}, url, KIO::Overwrite);
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
        QVERIFY(QFile::copy(QFINDTESTDATA("ftp/testOverwriteCopy1"), m_remoteDir.path() + path));

        // Without overwrite flag.
        // https://bugs.kde.org/show_bug.cgi?id=409954
        auto job = KIO::copy({QUrl::fromLocalFile(QFINDTESTDATA("ftp/testOverwriteCopy2"))}, url, KIO::DefaultFlags);
        job->setUiDelegate(nullptr);
        QVERIFY2(!job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->error(), KIO::ERR_FILE_ALREADY_EXIST);
        QFile file(m_remoteDir.path() + path);
        QVERIFY(file.exists());
        QVERIFY(file.open(QFile::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("testOverwriteCopy1\n")); // not 2!
    }

    void testDotEntry()
    {
        const QString path("/");
        const auto url = this->url(path);

        QStringList entryNames;
        auto job = KIO::listDir(url);
        job->setUiDelegate(nullptr);
        connect(job, &KIO::ListJob::entries, this, [&entryNames](KIO::Job *, const KIO::UDSEntryList &list) {
            for (const auto &entry : list) {
                entryNames << entry.stringValue(KIO::UDSEntry::UDS_NAME);
            }
        });

        QVERIFY2(job->exec(), qUtf8Printable(job->errorString()));
        QCOMPARE(job->error(), 0);
        QVERIFY(entryNames.contains(QStringLiteral(".")));
    }

    void testListingModes_data()
    {
        QTest::addColumn<QString>("listingMode");
        QTest::addColumn<bool>("readOnlyDirWritable");

        QTest::newRow("list") << QStringLiteral("list") << false;
        // MLST in FEAT but no MLSD: the LIST fallback is parsed as LIST.
        QTest::newRow("mlsd-refused") << QStringLiteral("mlsd-refused") << false;
    }

    void testListingModes()
    {
        QFETCH(QString, listingMode);
        QFETCH(bool, readOnlyDirWritable);

        QTemporaryDir remoteDir;
        const QDateTime fileTime(QDate(2026, 3, 4), QTime(5, 6, 7), QTimeZone::UTC);
        {
            QFile file(remoteDir.filePath(QStringLiteral("file")));
            QVERIFY(file.open(QFile::WriteOnly));
            file.write("12345");
            QVERIFY(file.flush());
            QVERIFY(file.setFileTime(fileTime, QFileDevice::FileModificationTime));
        }
        QVERIFY(QDir(remoteDir.path()).mkdir(QStringLiteral("writable")));
        QVERIFY(QDir(remoteDir.path()).mkdir(QStringLiteral("readonly")));
        QVERIFY(QFile::setPermissions(remoteDir.filePath(QStringLiteral("readonly")),
                                      QFile::ReadOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther));

        QProcess daemon;
        QUrl daemonUrl(QStringLiteral("ftp://localhost"));
        runDaemon(daemon, daemonUrl, remoteDir, listingMode);
        auto stopDaemon = qScopeGuard([&daemon] {
            daemon.kill();
            daemon.waitForFinished();
        });

        QHash<QString, KIO::UDSEntry> entries;
        daemonUrl.setPath(QStringLiteral("/"));
        auto listJob = KIO::listDir(daemonUrl, KIO::HideProgressInfo);
        listJob->setUiDelegate(nullptr);
        connect(listJob, &KIO::ListJob::entries, this, [&entries](KIO::Job *, const KIO::UDSEntryList &list) {
            for (const auto &entry : list) {
                entries.insert(entry.stringValue(KIO::UDSEntry::UDS_NAME), entry);
            }
        });
        QVERIFY2(listJob->exec(), qUtf8Printable(listJob->errorString()));

        QStringList names = entries.keys();
        names.sort();
        QCOMPARE(names, QStringList({QStringLiteral("."), QStringLiteral("file"), QStringLiteral("readonly"), QStringLiteral("writable")}));

        const auto isWritable = [](const KIO::UDSEntry &entry) {
            return (entry.numberValue(KIO::UDSEntry::UDS_ACCESS) & 0222) != 0;
        };
        const KIO::UDSEntry file = entries.value(QStringLiteral("file"));
        QCOMPARE(file.numberValue(KIO::UDSEntry::UDS_SIZE), 5);
        QVERIFY(isWritable(file));
        if (listingMode.startsWith(QLatin1String("mlsd-")) && listingMode != QLatin1String("mlsd-refused")) {
            // LIST only has minute precision
            QCOMPARE(file.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME), fileTime.toSecsSinceEpoch());
        }
        QVERIFY(entries.value(QStringLiteral("writable")).isDir());
        QVERIFY(isWritable(entries.value(QStringLiteral("writable"))));
        QVERIFY(entries.value(QStringLiteral("readonly")).isDir());
        QCOMPARE(isWritable(entries.value(QStringLiteral("readonly"))), readOnlyDirWritable);

        // stat of an existing file as a copy destination: it lists the parent with LIST <name>.
        daemonUrl.setPath(QStringLiteral("/file"));
        auto statJob = KIO::stat(daemonUrl, KIO::StatJob::DestinationSide, KIO::StatDefaultDetails, KIO::HideProgressInfo);
        statJob->setUiDelegate(nullptr);
        QVERIFY2(statJob->exec(), qUtf8Printable(statJob->errorString()));
        QCOMPARE(statJob->statResult().numberValue(KIO::UDSEntry::UDS_SIZE), 5);
    }
};

QTEST_MAIN(FTPTest)
#include "ftptest.moc"
