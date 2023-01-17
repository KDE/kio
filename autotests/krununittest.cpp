/*
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2007, 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#undef QT_USE_FAST_OPERATOR_PLUS

#include "krununittest.h"

#include <QSignalSpy>
#include <QTest>

QTEST_GUILESS_MAIN(KRunUnitTest)

#include <QStandardPaths>

#include "kiotesthelper.h" // createTestFile etc.
#include "krun.h"
#include <KApplicationTrader>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KProcess>
#include <KService>
#include <KSharedConfig>
#include <KShell>
#include <desktopexecparser.h>
#include <global.h>
#include <kprotocolinfo.h>
#ifdef Q_OS_UNIX
#include <signal.h> // kill
#endif

void KRunUnitTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    qputenv("PATH", QByteArray(qgetenv("PATH") + QFile::encodeName(QDir::listSeparator() + QCoreApplication::applicationDirPath())));

    // testProcessDesktopExec works only if your terminal application is set to "xterm"
    KConfigGroup cg(KSharedConfig::openConfig(), "General");
    cg.writeEntry("TerminalApplication", "true");

    // We just want to test if the command is properly constructed
    m_pseudoTerminalProgram = QStandardPaths::findExecutable(QStringLiteral("true"));
    QVERIFY(!m_pseudoTerminalProgram.isEmpty());

    // Determine the full path of sh - this is needed to make testProcessDesktopExecNoFile()
    // pass on systems where QStandardPaths::findExecutable("sh") is not "/bin/sh".
    m_sh = QStandardPaths::findExecutable(QStringLiteral("sh"));
    if (m_sh.isEmpty()) {
        m_sh = QStringLiteral("/bin/sh");
    }
}

void KRunUnitTest::cleanupTestCase()
{
    std::for_each(m_filesToRemove.begin(), m_filesToRemove.end(), [](const QString &f) {
        QFile::remove(f);
    });
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
class KRunImpl : public KRun
{
public:
    KRunImpl(const QUrl &url)
        : KRun(url, nullptr, false)
        , m_errCode(-1)
    {
    }

    void foundMimeType(const QString &type) override
    {
        m_mimeType = type;
        // don't call KRun::foundMimeType, we don't want to start an app ;-)
        setFinished(true);
    }

    void handleInitError(int kioErrorCode, const QString &err) override
    {
        m_errCode = kioErrorCode;
        m_errText = err;
    }

    QString mimeTypeFound() const
    {
        return m_mimeType;
    }
    int errorCode() const
    {
        return m_errCode;
    }
    QString errorText() const
    {
        return m_errText;
    }

private:
    int m_errCode;
    QString m_errText;
    QString m_mimeType;
};

void KRunUnitTest::testMimeTypeFile()
{
    const QString filePath = homeTmpDir() + "file";
    createTestFile(filePath, true);
    KRunImpl *krun = new KRunImpl(QUrl::fromLocalFile(filePath));
    krun->setAutoDelete(false);
    QSignalSpy spyFinished(krun, &KRun::finished);
    QVERIFY(spyFinished.wait(1000));
    QCOMPARE(krun->mimeTypeFound(), QString::fromLatin1("text/plain"));
    delete krun;
}

void KRunUnitTest::testMimeTypeDirectory()
{
    const QString dir = homeTmpDir() + "dir";
    createTestDirectory(dir);
    KRunImpl *krun = new KRunImpl(QUrl::fromLocalFile(dir));
    QSignalSpy spyFinished(krun, &KRun::finished);
    QVERIFY(spyFinished.wait(1000));
    QCOMPARE(krun->mimeTypeFound(), QString::fromLatin1("inode/directory"));
}

void KRunUnitTest::testMimeTypeBrokenLink()
{
    const QString dir = homeTmpDir() + "dir";
    createTestDirectory(dir);
    KRunImpl *krun = new KRunImpl(QUrl::fromLocalFile(dir + "/testlink"));
    QSignalSpy spyError(krun, &KRun::error);
    QSignalSpy spyFinished(krun, &KRun::finished);
    QVERIFY(spyFinished.wait(1000));
    QVERIFY(krun->mimeTypeFound().isEmpty());
    QCOMPARE(spyError.count(), 1);
    QCOMPARE(krun->errorCode(), int(KIO::ERR_DOES_NOT_EXIST));
    QVERIFY(krun->errorText().contains("does not exist"));
    QTest::qWait(100); // let auto-deletion proceed.
}

void KRunUnitTest::testMimeTypeDoesNotExist() // ported to OpenUrlJobTest::nonExistingFile()
{
    KRunImpl *krun = new KRunImpl(QUrl::fromLocalFile(QStringLiteral("/does/not/exist")));
    QSignalSpy spyError(krun, &KRun::error);
    QSignalSpy spyFinished(krun, &KRun::finished);
    QVERIFY(spyFinished.wait(1000));
    QVERIFY(krun->mimeTypeFound().isEmpty());
    QCOMPARE(spyError.count(), 1);
    QTest::qWait(100); // let auto-deletion proceed.
}

#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
static const char s_tempServiceName[] = "krununittest_service.desktop";

static void createSrcFile(const QString path)
{
    QFile srcFile(path);
    QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
    srcFile.write("Hello world\n");
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
void KRunUnitTest::KRunRunService_data()
{
    QTest::addColumn<bool>("tempFile");
    QTest::addColumn<bool>("useRunApplication");

    QTest::newRow("standard") << false << false;
    QTest::newRow("tempfile") << true << false;
    QTest::newRow("runApp") << false << true;
    QTest::newRow("runApp_tempfile") << true << true;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
void KRunUnitTest::KRunRunService()
{
    QFETCH(bool, tempFile);
    QFETCH(bool, useRunApplication);

    // Given a service desktop file and a source file
    const QString path = createTempService();
    // KService::Ptr service = KService::serviceByDesktopPath(s_tempServiceName);
    // QVERIFY(service);
    KService service(path);
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString srcFile = srcDir + "/srcfile";
    createSrcFile(srcFile);
    QVERIFY(QFile::exists(srcFile));
    QList<QUrl> urls;
    urls.append(QUrl::fromLocalFile(srcFile));

    // When calling KRun::runService or KRun::runApplication
    qint64 pid = useRunApplication ? KRun::runApplication(service, urls, nullptr, tempFile ? KRun::RunFlags(KRun::DeleteTemporaryFiles) : KRun::RunFlags())
                                   : KRun::runService(service, urls, nullptr, tempFile); // DEPRECATED

    // Then the service should be executed (which copies the source file to "dest")
    QVERIFY(pid != 0);
    const QString dest = srcDir + "/dest";
    QTRY_VERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(srcFile)); // if tempfile is true, kioexec will delete it... in 3 minutes.

    // All done, clean up.
    QVERIFY(QFile::remove(dest));
#ifdef Q_OS_UNIX
    ::kill(pid, SIGTERM);
#endif
}

QString KRunUnitTest::createTempService()
{
    // fakeservice: deleted and recreated by testKSycocaUpdate, don't use in other tests
    const QString fileName = s_tempServiceName;
    // bool mustUpdateKSycoca = !KService::serviceByDesktopPath(fileName);
    const QString fakeService = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kservices5/") + fileName;
    if (!QFile::exists(fakeService)) {
        // mustUpdateKSycoca = true;
        KDesktopFile file(fakeService);
        KConfigGroup group = file.desktopGroup();
        group.writeEntry("Name", "KRunUnittestService");
        group.writeEntry("Type", "Service");
#ifdef Q_OS_WIN
        group.writeEntry("Exec", "copy.exe %f %d/dest");
#else
        group.writeEntry("Exec", "cp %f %d/dest");
#endif
        file.sync();
        QFile f(fakeService);
        f.setPermissions(f.permissions() | QFile::ExeOwner | QFile::ExeUser);
    }
    m_filesToRemove.append(fakeService);
    return fakeService;
}

#endif
