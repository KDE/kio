/*
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2007, 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "desktopexecparsertest.h"

#include <QTest>

QTEST_GUILESS_MAIN(DesktopExecParserTest)

#include <QStandardPaths>

#include <desktopexecparser.h>

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KProcess>
#include <KService>
#include <KSharedConfig>
#include <KShell>

void DesktopExecParserTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    qputenv("PATH", QByteArray(qgetenv("PATH") + QFile::encodeName(QDir::listSeparator() + QCoreApplication::applicationDirPath())));

    // testProcessDesktopExec works only if your terminal application is set to "xterm"
    KConfigGroup cg(KSharedConfig::openConfig(), "General");
    cg.writeEntry("TerminalApplication", "true --test");

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

void DesktopExecParserTest::testExecutableName_data()
{
    QTest::addColumn<QString>("execLine");
    QTest::addColumn<QString>("expectedPath");
    QTest::addColumn<QString>("expectedName");

    QTest::newRow("/usr/bin/ls") << "/usr/bin/ls"
                                 << "/usr/bin/ls"
                                 << "ls";
    QTest::newRow("/path/to/wine \"long argument with path\"") << "/path/to/wine \"long argument with path\""
                                                               << "/path/to/wine"
                                                               << "wine";
    QTest::newRow("/path/with/a/sp\\ ace/exe arg1 arg2") << "/path/with/a/sp\\ ace/exe arg1 arg2"
                                                         << "/path/with/a/sp ace/exe"
                                                         << "exe";
    QTest::newRow("\"progname\" \"arg1\"") << "\"progname\" \"arg1\""
                                           << "progname"
                                           << "progname";
    QTest::newRow("'quoted' \"arg1\"") << "'quoted' \"arg1\""
                                       << "quoted"
                                       << "quoted";
    QTest::newRow(" 'leading space'   arg1") << " 'leading space'   arg1"
                                             << "leading space"
                                             << "leading space";
    QTest::newRow("if_command") << "if test -e /tmp/foo; then kwrite ; else konsole ; fi"
                                << ""
                                << ""; // "if" isn't a known executable, so this is good...
}

void DesktopExecParserTest::testExecutableName()
{
    QFETCH(QString, execLine);
    QFETCH(QString, expectedPath);
    QFETCH(QString, expectedName);
    QCOMPARE(KIO::DesktopExecParser::executableName(execLine), expectedName);
    QCOMPARE(KIO::DesktopExecParser::executablePath(execLine), expectedPath);
}

// static const char *bt(bool tr) { return tr?"true":"false"; }
static void checkDesktopExecParser(const char *exec, const char *term, const char *sus, const QList<QUrl> &urls, bool tf, const QString &b)
{
    QFile out(QStringLiteral("kruntest.desktop"));
    if (!out.open(QIODevice::WriteOnly)) {
        abort();
    }
    QByteArray str(
        "[Desktop Entry]\n"
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
        service.exec().toLatin1().constData(), bt(service.terminal()), service.terminalOptions().toLatin1().constData(), bt(service.substituteUid()),
       service.username().toLatin1().constData(), KShell::joinArgs(urls.toStringList()).toLatin1().constData(), bt(tf));
    */
    KIO::DesktopExecParser parser(service, urls);
    parser.setUrlsAreTempFiles(tf);
    QCOMPARE(KShell::joinArgs(parser.resultingArguments()), b);

    QFile::remove(QStringLiteral("kruntest.desktop"));
}

void DesktopExecParserTest::testProcessDesktopExec()
{
    QList<QUrl> l0;
    static const char *const execs[] = {"Exec=date -u", "Exec=echo $PWD"};
    static const char *const terms[] = {"Terminal=false", "Terminal=true\nTerminalOptions=-T \"%f - %c\""};
    static const char *const sus[] = {"X-KDE-SubstituteUID=false", "X-KDE-SubstituteUID=true\nX-KDE-Username=sprallo"};
    static const char *const results[] = {
        "/bin/date -u", // 0
        "/bin/sh -c 'echo $PWD '", // 1
        "/bin/true --test -T ' - just_a_test' -e /bin/date -u", // 2
        "/bin/true --test -T ' - just_a_test' -e /bin/sh -c 'echo $PWD '", // 3
        /* kdesu */ " -u sprallo -c '/bin/date -u'", // 4
        /* kdesu */ " -u sprallo -c '/bin/sh -c '\\''echo $PWD '\\'''", // 5
        "/bin/true --test -T ' - just_a_test' -e su sprallo -c '/bin/date -u'", // 6
        "/bin/true --test -T ' - just_a_test' -e su sprallo -c '/bin/sh -c '\\''echo $PWD '\\'''", // 7
    };

    // Find out the full path of the shell which will be used to execute shell commands
    KProcess process;
    process.setShellCommand(QLatin1String(""));
    const QString shellPath = process.program().at(0);

    // Arch moved /bin/date to /usr/bin/date...
    const QString datePath = QStandardPaths::findExecutable(QStringLiteral("date"));

    for (int su = 0; su < 2; su++) {
        for (int te = 0; te < 2; te++) {
            for (int ex = 0; ex < 2; ex++) {
                int pt = ex + te * 2 + su * 4;
                QString exe;
                if (pt == 4 || pt == 5) {
                    exe = QFile::decodeName(KDE_INSTALL_FULL_LIBEXECDIR_KF "/kdesu");
                    if (!QFile::exists(exe)) {
                        qWarning() << "kdesu not found, skipping test";
                        continue;
                    }
                }
                const QString result = QString::fromLatin1(results[pt])
                                           .replace(QLatin1String("/bin/true"), m_pseudoTerminalProgram)
                                           .replace(QLatin1String("/bin/sh"), shellPath)
                                           .replace(QLatin1String("/bin/date"), datePath);
                checkDesktopExecParser(execs[ex], terms[te], sus[su], l0, false, exe + result);
            }
        }
    }
}

void DesktopExecParserTest::testProcessDesktopExecNoFile_data()
{
    /* clang-format off */
    QTest::addColumn<QString>("execLine");
    QTest::addColumn<QList<QUrl>>("urls");
    QTest::addColumn<bool>("tempfiles");
    QTest::addColumn<QString>("expected");

    QList<QUrl> l0;
    QList<QUrl> l1;
    l1 << QUrl(QStringLiteral("file:/tmp"));
    QList<QUrl> l2;
    l2 << QUrl(QStringLiteral("http://localhost/foo"));
    QList<QUrl> l3;
    l3 << QUrl(QStringLiteral("file:/local/some file")) << QUrl(QStringLiteral("http://remotehost.org/bar"));
    QList<QUrl> l4;
    l4 << QUrl(QStringLiteral("http://login:password@www.kde.org"));

    // A real-world use case would be kate.
    // But I picked ktrash5 since it's installed by kio
    QString ktrash = QStandardPaths::findExecutable(QStringLiteral("ktrash5"));
    QVERIFY(!ktrash.isEmpty());
    QString ktrashQuoted = KShell::quoteArg(ktrash);

    QString kioexec = QCoreApplication::applicationDirPath() + "/kioexec";
    if (!QFileInfo::exists(kioexec)) {
        kioexec = KDE_INSTALL_FULL_LIBEXECDIR_KF "/kioexec";
    }
    QVERIFY(QFileInfo::exists(kioexec));
    QString kioexecQuoted = KShell::quoteArg(kioexec);

    QTest::newRow("%U l0") << "ktrash5 %U" << l0 << false << ktrashQuoted;
    QTest::newRow("%U l1") << "ktrash5 %U" << l1 << false << ktrashQuoted + " /tmp";
    QTest::newRow("%U l2") << "ktrash5 %U" << l2 << false << ktrashQuoted + " http://localhost/foo";
    QTest::newRow("%U l3") << "ktrash5 %U" << l3 << false << ktrashQuoted + " '/local/some file' http://remotehost.org/bar";

    // QTest::newRow("%u l0") << "ktrash5 %u" << l0 << false << ktrashQuoted; // gives runtime warning
    QTest::newRow("%u l1") << "ktrash5 %u" << l1 << false << ktrashQuoted + " /tmp";
    QTest::newRow("%u l2") << "ktrash5 %u" << l2 << false << ktrashQuoted + " http://localhost/foo";
    // QTest::newRow("%u l3") << "ktrash5 %u" << l3 << false << ktrashQuoted; // gives runtime warning

    QTest::newRow("%F l0") << "ktrash5 %F" << l0 << false << ktrashQuoted;
    QTest::newRow("%F l1") << "ktrash5 %F" << l1 << false << ktrashQuoted + " /tmp";
    QTest::newRow("%F l2") << "ktrash5 %F" << l2 << false << kioexecQuoted + " 'ktrash5 %F' http://localhost/foo";
    QTest::newRow("%F l3") << "ktrash5 %F" << l3 << false << kioexecQuoted + " 'ktrash5 %F' 'file:///local/some file' http://remotehost.org/bar";

    QTest::newRow("%F l1 tempfile") << "ktrash5 %F" << l1 << true << kioexecQuoted + " --tempfiles 'ktrash5 %F' file:///tmp";
    QTest::newRow("%f l1 tempfile") << "ktrash5 %f" << l1 << true << kioexecQuoted + " --tempfiles 'ktrash5 %f' file:///tmp";

    QTest::newRow("sh -c ktrash5 %F")
        << R"(sh -c "ktrash5 "'\"'"%F"'\"')"
        << l1
        << false
        << m_sh + R"( -c 'ktrash5 \"/tmp\"')";

    // This was originally with kmailservice5, but that relies on it being installed
    QTest::newRow("ktrash5 %u l1") << "ktrash5 %u" << l1 << false << ktrashQuoted + " /tmp";
    QTest::newRow("ktrash5 %u l4") << "ktrash5 %u" << l4 << false << ktrashQuoted + " http://login:password@www.kde.org";

    /* clang-format on */
}

void DesktopExecParserTest::testProcessDesktopExecNoFile()
{
    QFETCH(QString, execLine);
    KService service(QStringLiteral("dummy"), execLine, QStringLiteral("app"));
    QFETCH(QList<QUrl>, urls);
    QFETCH(bool, tempfiles);
    QFETCH(QString, expected);
    KIO::DesktopExecParser parser(service, urls);
    parser.setUrlsAreTempFiles(tempfiles);
    const QStringList args = parser.resultingArguments();
    QVERIFY2(!args.isEmpty(), qPrintable(parser.errorMessage()));
    QCOMPARE(KShell::joinArgs(args), expected);
}

extern KSERVICE_EXPORT int ksycoca_ms_between_checks;

void DesktopExecParserTest::testKtelnetservice()
{
    const QString ktelnetDesk = QFINDTESTDATA(QStringLiteral("../src/schemehandlers/telnet/ktelnetservice5.desktop"));
    QVERIFY(!ktelnetDesk.isEmpty());

    // KApplicationTrader in KIO::DesktopExecParser::hasSchemeHandler() needs the .desktop file to be installed
    const QString destDir = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    QVERIFY(QDir().mkpath(destDir));
    QFile::remove(destDir + QLatin1String("/ktelnetservice5.desktop"));
    QVERIFY(QFile::copy(ktelnetDesk, destDir + QLatin1String("/ktelnetservice5.desktop")));

    ksycoca_ms_between_checks = 0; // need it to check the ksycoca mtime

    KService::Ptr service = KService::serviceByStorageId(QStringLiteral("ktelnetservice5.desktop"));
    QVERIFY(service);

    QString ktelnetExec = QStandardPaths::findExecutable(QStringLiteral("ktelnetservice5"));
    // if KIO is installed we'll find <bindir>/ktelnetservice5, otherwise KIO::DesktopExecParser will
    // use the executable from Exec= line
    if (ktelnetExec.isEmpty()) {
        ktelnetExec = service->exec().remove(QLatin1String(" %u"));
    }
    QVERIFY(!ktelnetExec.isEmpty());

    const QString expected = KShell::quoteArg(ktelnetExec) + QLatin1String(" %1://root@10.1.1.1");
    const QStringList protocols({QStringLiteral("ssh"), QStringLiteral("telnet"), QStringLiteral("rlogin")});
    for (const QString &protocol : protocols) {
        const QList<QUrl> urls({QUrl(QStringLiteral("%1://root@10.1.1.1").arg(protocol))});
        KIO::DesktopExecParser parser(*service, urls);
        QCOMPARE(KShell::joinArgs(parser.resultingArguments()), expected.arg(protocol));
    }
}
