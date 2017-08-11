/*
 *  Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
 *  Copyright (C) 2007, 2009 David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#undef QT_USE_FAST_OPERATOR_PLUS
#undef QT_USE_FAST_CONCATENATION

#include "krununittest.h"

#include <QtTest/QtTest>

QTEST_GUILESS_MAIN(KRunUnitTest)

#include <qstandardpaths.h>

#include "krun.h"
#include <desktopexecparser.h>
#include <kshell.h>
#include <kservice.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>
#include <kprocess.h>
#include <KDesktopFile>
#include "kiotesthelper.h" // createTestFile etc.
#ifdef Q_OS_UNIX
#include <signal.h> // kill
#endif

void KRunUnitTest::initTestCase()
{
    QStandardPaths::enableTestMode(true);
    // testProcessDesktopExec works only if your terminal application is set to "x-term"
    KConfigGroup cg(KSharedConfig::openConfig(), "General");
    cg.writeEntry("TerminalApplication", "x-term");

    // Determine the full path of sh - this is needed to make testProcessDesktopExecNoFile()
    // pass on systems where QStandardPaths::findExecutable("sh") is not "/bin/sh".
    m_sh = QStandardPaths::findExecutable(QStringLiteral("sh"));
    if (m_sh.isEmpty()) {
        m_sh = QStringLiteral("/bin/sh");
    }
}

void KRunUnitTest::cleanupTestCase()
{
    std::for_each(m_filesToRemove.begin(), m_filesToRemove.end(), [](const QString & f) {
        QFile::remove(f);
    });
}

void KRunUnitTest::testBinaryName_data()
{
    QTest::addColumn<QString>("execLine");
    QTest::addColumn<bool>("removePath");
    QTest::addColumn<QString>("expected");

    QTest::newRow("/usr/bin/ls true") << "/usr/bin/ls" << true << "ls";
    QTest::newRow("/usr/bin/ls false") << "/usr/bin/ls" << false << "/usr/bin/ls";
    QTest::newRow("/path/to/wine \"long argument with path\"") << "/path/to/wine \"long argument with path\"" << true << "wine";
    QTest::newRow("/path/with/a/sp\\ ace/exe arg1 arg2") << "/path/with/a/sp\\ ace/exe arg1 arg2" << true << "exe";
    QTest::newRow("\"progname\" \"arg1\"") << "\"progname\" \"arg1\"" << true << "progname";
    QTest::newRow("'quoted' \"arg1\"") << "'quoted' \"arg1\"" << true << "quoted";
    QTest::newRow(" 'leading space'   arg1") << " 'leading space'   arg1" << true << "leading space";
}

void KRunUnitTest::testBinaryName()
{
    QFETCH(QString, execLine);
    QFETCH(bool, removePath);
    QFETCH(QString, expected);
    if (removePath) {
        QCOMPARE(KIO::DesktopExecParser::executableName(execLine), expected);
    } else {
        QCOMPARE(KIO::DesktopExecParser::executablePath(execLine), expected);
    }
}

//static const char *bt(bool tr) { return tr?"true":"false"; }
static void checkDesktopExecParser(const char *exec, const char *term, const char *sus,
                                   const QList<QUrl> &urls, bool tf, const QString &b)
{
    QFile out(QStringLiteral("kruntest.desktop"));
    if (!out.open(QIODevice::WriteOnly)) {
        abort();
    }
    QByteArray str("[Desktop Entry]\n"
                   "Type=Application\n"
                   "Name=just_a_test\n"
                   "Icon=~/icon.png\n");
    str += QByteArray(exec) + '\n';
    str += QByteArray(term) + '\n';
    str += QByteArray(sus) + '\n';
    out.write(str);
    out.close();

    KService service(QDir::currentPath() + "/kruntest.desktop");
    /*qDebug() << QString().sprintf(
        "processDesktopExec( "
        "service = {\nexec = %s\nterminal = %s, terminalOptions = %s\nsubstituteUid = %s, user = %s },"
        "\nURLs = { %s },\ntemp_files = %s )",
        service.exec().toLatin1().constData(), bt(service.terminal()), service.terminalOptions().toLatin1().constData(), bt(service.substituteUid()), service.username().toLatin1().constData(),
        KShell::joinArgs(urls.toStringList()).toLatin1().constData(), bt(tf));
    */
    KIO::DesktopExecParser parser(service, urls);
    parser.setUrlsAreTempFiles(tf);
    QCOMPARE(KShell::joinArgs(parser.resultingArguments()), b);

    QFile::remove(QStringLiteral("kruntest.desktop"));
}

void KRunUnitTest::testProcessDesktopExec()
{
    QList<QUrl> l0;
    static const char *const execs[] = { "Exec=date -u", "Exec=echo $PWD" };
    static const char *const terms[] = { "Terminal=false", "Terminal=true\nTerminalOptions=-T \"%f - %c\"" };
    static const char *const sus[] = { "X-KDE-SubstituteUID=false", "X-KDE-SubstituteUID=true\nX-KDE-Username=sprallo" };
    static const char *const results[] = {
        "/bin/date -u", // 0
        "/bin/sh -c 'echo $PWD '", // 1
        "x-term -T ' - just_a_test' -e /bin/date -u", // 2
        "x-term -T ' - just_a_test' -e /bin/sh -c 'echo $PWD '", // 3
        /* kdesu */ " -u sprallo -c '/bin/date -u'", // 4
        /* kdesu */ " -u sprallo -c '/bin/sh -c '\\''echo $PWD '\\'''", // 5
        "x-term -T ' - just_a_test' -e su sprallo -c '/bin/date -u'", // 6
        "x-term -T ' - just_a_test' -e su sprallo -c '/bin/sh -c '\\''echo $PWD '\\'''", // 7
    };

    // Find out the full path of the shell which will be used to execute shell commands
    KProcess process;
    process.setShellCommand(QLatin1String(""));
    const QString shellPath = process.program().at(0);

    // Arch moved /bin/date to /usr/bin/date...
    const QString datePath = QStandardPaths::findExecutable(QStringLiteral("date"));

    for (int su = 0; su < 2; su++)
        for (int te = 0; te < 2; te++)
            for (int ex = 0; ex < 2; ex++) {
                int pt = ex + te * 2 + su * 4;
                QString exe;
                if (pt == 4 || pt == 5) {
                    exe = QFile::decodeName(CMAKE_INSTALL_FULL_LIBEXECDIR_KF5 "/kdesu");
                    if (!QFile::exists(exe)) {
                        qWarning() << "kdesu not found, skipping test";
                        continue;
                    }
                }
                const QString result = QString::fromLatin1(results[pt])
                                       .replace(QLatin1String("/bin/sh"), shellPath)
                                       .replace(QLatin1String("/bin/date"), datePath);
                checkDesktopExecParser(execs[ex], terms[te], sus[su], l0, false, exe + result);
            }
}

void KRunUnitTest::testProcessDesktopExecNoFile_data()
{
    QTest::addColumn<QString>("execLine");
    QTest::addColumn<QList<QUrl> >("urls");
    QTest::addColumn<bool>("tempfiles");
    QTest::addColumn<QString>("expected");

    QList<QUrl> l0;
    QList<QUrl> l1; l1 << QUrl(QStringLiteral("file:/tmp"));
    QList<QUrl> l2; l2 << QUrl(QStringLiteral("http://localhost/foo"));
    QList<QUrl> l3; l3 << QUrl(QStringLiteral("file:/local/some file")) << QUrl(QStringLiteral("http://remotehost.org/bar"));
    QList<QUrl> l4; l4 << QUrl(QStringLiteral("http://login:password@www.kde.org"));

    // A real-world use case would be kate.
    // But I picked kdeinit5 since it's installed by kdelibs
    QString kdeinit = QStandardPaths::findExecutable(QStringLiteral("kdeinit5"));
    if (kdeinit.isEmpty()) {
        kdeinit = QStringLiteral("kdeinit5");
    }

    QString kioexec = QCoreApplication::applicationDirPath() + "/kioexec";
    if (!QFileInfo::exists(kioexec)) {
        kioexec = CMAKE_INSTALL_FULL_LIBEXECDIR_KF5 "/kioexec";
    }
    QVERIFY(QFileInfo::exists(kioexec));
    QString kioexecQuoted = KShell::quoteArg(kioexec);

    QString kmailservice = QStandardPaths::findExecutable(QStringLiteral("kmailservice5"));
    if (!QFile::exists(kmailservice)) {
        kmailservice = QStringLiteral("kmailservice5");
    }

    QTest::newRow("%U l0") << "kdeinit5 %U" << l0 << false << kdeinit;
    QTest::newRow("%U l1") << "kdeinit5 %U" << l1 << false << kdeinit + " /tmp";
    QTest::newRow("%U l2") << "kdeinit5 %U" << l2 << false << kdeinit + " http://localhost/foo";
    QTest::newRow("%U l3") << "kdeinit5 %U" << l3 << false << kdeinit + " '/local/some file' http://remotehost.org/bar";

    //QTest::newRow("%u l0") << "kdeinit5 %u" << l0 << false << kdeinit; // gives runtime warning
    QTest::newRow("%u l1") << "kdeinit5 %u" << l1 << false << kdeinit + " /tmp";
    QTest::newRow("%u l2") << "kdeinit5 %u" << l2 << false << kdeinit + " http://localhost/foo";
    //QTest::newRow("%u l3") << "kdeinit5 %u" << l3 << false << kdeinit; // gives runtime warning

    QTest::newRow("%F l0") << "kdeinit5 %F" << l0 << false << kdeinit;
    QTest::newRow("%F l1") << "kdeinit5 %F" << l1 << false << kdeinit + " /tmp";
    QTest::newRow("%F l2") << "kdeinit5 %F" << l2 << false << kioexecQuoted + " 'kdeinit5 %F' http://localhost/foo";
    QTest::newRow("%F l3") << "kdeinit5 %F" << l3 << false << kioexecQuoted + " 'kdeinit5 %F' 'file:///local/some file' http://remotehost.org/bar";

    QTest::newRow("%F l1 tempfile") << "kdeinit5 %F" << l1 << true << kioexecQuoted + " --tempfiles 'kdeinit5 %F' file:///tmp";
    QTest::newRow("%f l1 tempfile") << "kdeinit5 %f" << l1 << true << kioexecQuoted + " --tempfiles 'kdeinit5 %f' file:///tmp";

    QTest::newRow("sh -c kdeinit5 %F") << "sh -c \"kdeinit5 \"'\\\"'\"%F\"'\\\"'"
                                       << l1 << false << m_sh + " -c 'kdeinit5 \\\"/tmp\\\"'";

    QTest::newRow("kmailservice5 %u l1") << "kmailservice5 %u" << l1 << false << kmailservice + " /tmp";
    QTest::newRow("kmailservice5 %u l4") << "kmailservice5 %u" << l4 << false << kmailservice + " http://login:password@www.kde.org";
}

void KRunUnitTest::testProcessDesktopExecNoFile()
{
    QFETCH(QString, execLine);
    KService service(QStringLiteral("dummy"), execLine, QStringLiteral("app"));
    QFETCH(QList<QUrl>, urls);
    QFETCH(bool, tempfiles);
    QFETCH(QString, expected);
    KIO::DesktopExecParser parser(service, urls);
    parser.setUrlsAreTempFiles(tempfiles);
    QCOMPARE(KShell::joinArgs(parser.resultingArguments()), expected);
}

class KRunImpl : public KRun
{
public:
    KRunImpl(const QUrl &url)
        : KRun(url, nullptr, false), m_errCode(-1) {}

    void foundMimeType(const QString &type) Q_DECL_OVERRIDE {
        m_mimeType = type;
        // don't call KRun::foundMimeType, we don't want to start an app ;-)
        setFinished(true);
    }

    void handleInitError(int kioErrorCode, const QString &err) Q_DECL_OVERRIDE {
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
    QSignalSpy spyFinished(krun, SIGNAL(finished()));
    QVERIFY(spyFinished.wait(1000));
    QCOMPARE(krun->mimeTypeFound(), QString::fromLatin1("text/plain"));
    delete krun;
}

void KRunUnitTest::testMimeTypeDirectory()
{
    const QString dir = homeTmpDir() + "dir";
    createTestDirectory(dir);
    KRunImpl *krun = new KRunImpl(QUrl::fromLocalFile(dir));
    QSignalSpy spyFinished(krun, SIGNAL(finished()));
    QVERIFY(spyFinished.wait(1000));
    QCOMPARE(krun->mimeTypeFound(), QString::fromLatin1("inode/directory"));
}

void KRunUnitTest::testMimeTypeBrokenLink()
{
    const QString dir = homeTmpDir() + "dir";
    createTestDirectory(dir);
    KRunImpl *krun = new KRunImpl(QUrl::fromLocalFile(dir + "/testlink"));
    QSignalSpy spyError(krun, SIGNAL(error()));
    QSignalSpy spyFinished(krun, SIGNAL(finished()));
    QVERIFY(spyFinished.wait(1000));
    QVERIFY(krun->mimeTypeFound().isEmpty());
    QCOMPARE(spyError.count(), 1);
    QCOMPARE(krun->errorCode(), int(KIO::ERR_DOES_NOT_EXIST));
    QVERIFY(krun->errorText().contains("does not exist"));
    QTest::qWait(100); // let auto-deletion proceed.
}

void KRunUnitTest::testMimeTypeDoesNotExist()
{
    KRunImpl *krun = new KRunImpl(QUrl::fromLocalFile(QStringLiteral("/does/not/exist")));
    QSignalSpy spyError(krun, SIGNAL(error()));
    QSignalSpy spyFinished(krun, SIGNAL(finished()));
    QVERIFY(spyFinished.wait(1000));
    QVERIFY(krun->mimeTypeFound().isEmpty());
    QCOMPARE(spyError.count(), 1);
    QTest::qWait(100); // let auto-deletion proceed.
}

static const char s_tempServiceName[] = "krununittest_service.desktop";

static void createSrcFile(const QString path)
{
    QFile srcFile(path);
    QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
    srcFile.write("Hello world\n");
}

void KRunUnitTest::KRunRunService_data()
{
    QTest::addColumn<bool>("tempFile");
    QTest::addColumn<bool>("useRunApplication");

    QTest::newRow("standard") << false << false;
    QTest::newRow("tempfile") << true << false;
    QTest::newRow("runApp") << false << true;
    QTest::newRow("runApp_tempfile") << true << true;
}
void KRunUnitTest::KRunRunService()
{
    QFETCH(bool, tempFile);
    QFETCH(bool, useRunApplication);

    // Given a service desktop file and a source file
    const QString path = createTempService();
    //KService::Ptr service = KService::serviceByDesktopPath(s_tempServiceName);
    //QVERIFY(service);
    KService service(path);
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString srcFile = srcDir + "/srcfile";
    createSrcFile(srcFile);
    QVERIFY(QFile::exists(srcFile));
    QList<QUrl> urls;
    urls.append(QUrl::fromLocalFile(srcFile));

    // When calling KRun::runService or KRun::runApplication
    qint64 pid = useRunApplication
        ? KRun::runApplication(service, urls, nullptr, tempFile ? KRun::RunFlags(KRun::DeleteTemporaryFiles) : KRun::RunFlags())
        : KRun::runService(service, urls, nullptr, tempFile);

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
    //bool mustUpdateKSycoca = !KService::serviceByDesktopPath(fileName);
    const QString fakeService = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kservices5/") + fileName;
    if (!QFile::exists(fakeService)) {
        //mustUpdateKSycoca = true;
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
