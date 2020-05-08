/*
    This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "openurljobtest.h"
#include "openurljob.h"
#include <kprocessrunner_p.h>
#include <KApplicationTrader>

#include "kiotesthelper.h" // createTestFile etc.

#include <kservice.h>
#include <kconfiggroup.h>
#include <KDesktopFile>

#ifdef Q_OS_UNIX
#include <signal.h> // kill
#endif

#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <untrustedprogramhandlerinterface.h>

QTEST_GUILESS_MAIN(OpenUrlJobTest)

extern KSERVICE_EXPORT int ksycoca_ms_between_checks;

namespace KIO {
KIOGUI_EXPORT void setDefaultUntrustedProgramHandler(KIO::UntrustedProgramHandlerInterface *iface);
}
class TestUntrustedProgramHandler : public KIO::UntrustedProgramHandlerInterface
{
public:
    void showUntrustedProgramWarning(KJob *job, const QString &programName) override {
        Q_UNUSED(job)
        m_calls << programName;
        Q_EMIT result(m_retVal);
    }

    void setRetVal(bool b) { m_retVal = b; }

    QStringList m_calls;
    bool m_retVal = false;
};

static TestUntrustedProgramHandler s_handler;

static const char s_tempServiceName[] = "openurljobtest_service.desktop";

void OpenUrlJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // Ensure no leftovers from other tests
    QDir(QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)).removeRecursively();
    // (including a mimeapps.list file)
    // Don't remove ConfigLocation completely, it's useful when enabling debug output with kdebugsettings --test-mode
    const QString mimeApps = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QLatin1String("/mimeapps.list");
    QFile::remove(mimeApps);

    ksycoca_ms_between_checks = 0; // need it to check the ksycoca mtime
    const QString fakeService = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') + s_tempServiceName;
    writeApplicationDesktopFile(fakeService);
    m_filesToRemove.append(fakeService);

    // Ensure our service is the preferred one
    KConfig mimeAppsCfg(mimeApps);
    KConfigGroup grp = mimeAppsCfg.group("Default Applications");
    grp.writeEntry("text/plain", s_tempServiceName);
    grp.writeEntry("text/html", s_tempServiceName);
    grp.writeEntry("application/x-shellscript", s_tempServiceName);
    grp.sync();

    for (const char *mimeType : {"text/plain", "application/x-shellscript"}) {
        KService::Ptr preferredTextEditor = KApplicationTrader::preferredService(QString::fromLatin1(mimeType));
        QVERIFY(preferredTextEditor);
        QCOMPARE(preferredTextEditor->entryPath(), fakeService);
    }

    // As used for preferredService
    QVERIFY(KService::serviceByDesktopName("openurljobtest_service"));

    ksycoca_ms_between_checks = 5000; // all done, speed up again
}

void OpenUrlJobTest::cleanupTestCase()
{
    for (const QString &file : m_filesToRemove) {
        QFile::remove(file);
    };
}

void OpenUrlJobTest::init()
{
    QFile::remove(m_tempDir.path() + "/dest");
}

static void createSrcFile(const QString &path)
{
    QFile srcFile(path);
    QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
    srcFile.write("Hello world\n");
}

static QString readFile(const QString &path)
{
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    return QString::fromLocal8Bit(file.readAll()).trimmed();
}

void OpenUrlJobTest::startProcess_data()
{
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QString>("fileName");

    // Known mimetype
    QTest::newRow("text_file") << "text/plain" << "srcfile.txt";
    QTest::newRow("directory_file") << "application/x-desktop" << ".directory";
    QTest::newRow("desktop_file_link") << "application/x-desktop" << "srcfile.txt";
    QTest::newRow("desktop_file_link_preferred_service") << "application/x-desktop" << "srcfile.html";
    QTest::newRow("non_executable_script_running_not_allowed") << "application/x-shellscript" << "srcfile.sh";
    QTest::newRow("executable_script_running_not_allowed") << "application/x-shellscript" << "srcfile.sh";

    // Require mimetype determination
    QTest::newRow("text_file_no_mimetype") << QString() << "srcfile.txt";
    QTest::newRow("directory_file_no_mimetype") << QString() << ".directory";
}

void OpenUrlJobTest::startProcess()
{
    QFETCH(QString, mimeType);
    QFETCH(QString, fileName);

    // Given a file to open
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString srcFile = srcDir + QLatin1Char('/') + fileName;
    createSrcFile(srcFile);
    QVERIFY(QFile::exists(srcFile));
    const bool isLink = QByteArray(QTest::currentDataTag()).startsWith("desktop_file_link");
    QUrl url = QUrl::fromLocalFile(srcFile);
    if (isLink) {
        const QString desktopFilePath = srcDir + QLatin1String("/link.desktop");
        KDesktopFile linkDesktopFile(desktopFilePath);
        linkDesktopFile.desktopGroup().writeEntry("Type", "Link");
        linkDesktopFile.desktopGroup().writeEntry("URL", url);
        const bool linkHasPreferredService = QByteArray(QTest::currentDataTag()) == "desktop_file_link_preferred_service";
        if (linkHasPreferredService) {
            linkDesktopFile.desktopGroup().writeEntry("X-KDE-LastOpenedWith", "openurljobtest_service");
        }
        url = QUrl::fromLocalFile(desktopFilePath);
    }
    if (QByteArray(QTest::currentDataTag()).startsWith("executable")) {
        QFile file(srcFile);
        QVERIFY(file.setPermissions(QFile::ExeUser | file.permissions()));
    }

    // When running a OpenUrlJob
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(url, mimeType, this);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    // Then the service should be executed (which writes to "dest")
    const QString dest = m_tempDir.path() + "/dest";
    QTRY_VERIFY2(QFile::exists(dest), qPrintable(dest));
    QCOMPARE(readFile(dest), srcFile);
}

void OpenUrlJobTest::noServiceNoHandler()
{
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QUrl url = QUrl::fromLocalFile(tempFile.fileName());
    const QString mimeType = QStringLiteral("application/x-zerosize");
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(url, mimeType, this);
    // This is going to try QDesktopServices::openUrl which will fail because we are no QGuiApplication, good.
    QTest::ignoreMessage(QtWarningMsg, "QDesktopServices::openUrl: Application is not a GUI application");
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KJob::UserDefinedError);
    QCOMPARE(job->errorString(), QStringLiteral("Failed to open the file."));
}

void OpenUrlJobTest::invalidUrl()
{
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl(":/"), QStringLiteral("text/plain"), this);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_MALFORMED_URL);
    QCOMPARE(job->errorString(), QStringLiteral("Malformed URL\nRelative URL's path component contains ':' before any '/'; source was \":/\"; path = \":/\""));

    QUrl u;
    u.setPath(QStringLiteral("/pathonly"));
    KIO::OpenUrlJob *job2 = new KIO::OpenUrlJob(u, QStringLiteral("text/plain"), this);
    QVERIFY(!job2->exec());
    QCOMPARE(job2->error(), KIO::ERR_MALFORMED_URL);
    QCOMPARE(job2->errorString(), QStringLiteral("Malformed URL\n/pathonly"));
}

void OpenUrlJobTest::refuseRunningNativeExecutables()
{
   KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(QCoreApplication::applicationFilePath()), QStringLiteral("application/x-executable"), this);
   QVERIFY(!job->exec());
   QCOMPARE(job->error(), KJob::UserDefinedError);
   QVERIFY2(job->errorString().contains("For safety it will not be started"), qPrintable(job->errorString()));
}

void OpenUrlJobTest::refuseRunningRemoteNativeExecutables()
{
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl("protocol://host/path/exe"), QStringLiteral("application/x-executable"), this);
    job->setRunExecutables(true); // even with this enabled, an error will occur
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KJob::UserDefinedError);
    QVERIFY2(job->errorString().contains("For safety it will not be started"), qPrintable(job->errorString()));
}

KCONFIGCORE_EXPORT void loadUrlActionRestrictions(const KConfigGroup &cg);

void OpenUrlJobTest::notAuthorized()
{
    KConfigGroup cg(KSharedConfig::openConfig(), "KDE URL Restrictions");
    cg.writeEntry("rule_count", 1);
    cg.writeEntry("rule_1", QStringList{"open", {}, {}, {}, "file", "", "", "false"});
    cg.sync();
    loadUrlActionRestrictions(cg);

    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl("file:///"), QStringLiteral("text/plain"), this);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_ACCESS_DENIED);
    QCOMPARE(job->errorString(), QStringLiteral("Access denied to file:///."));

    cg.deleteEntry("rule_1");
    cg.deleteEntry("rule_count");
    cg.sync();
    loadUrlActionRestrictions(cg);
}

void OpenUrlJobTest::runScript_data()
{
    QTest::addColumn<QString>("mimeType");

    QTest::newRow("shellscript") << "application/x-shellscript";
    QTest::newRow("native") << "application/x-executable";
}

void OpenUrlJobTest::runScript()
{
#ifdef Q_OS_UNIX
    QFETCH(QString, mimeType);

    // Given an executable shell script that copies "src" to "dest"
    QTemporaryDir tempDir;
    const QString dir = tempDir.path();
    createSrcFile(dir + QLatin1String("/src"));
    const QString scriptFile = dir + QLatin1String("/script.sh");
    QFile file(scriptFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\ncp src dest");
    file.close();
    QVERIFY(file.setPermissions(QFile::ExeUser | file.permissions()));

    // When using OpenUrlJob to run the script
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(scriptFile), mimeType, this);
    job->setRunExecutables(true); // startProcess tests the case where this isn't set

    // Then it works :-)
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QTRY_VERIFY(QFileInfo::exists(dir + QLatin1String("/dest"))); // TRY because CommandLineLauncherJob finishes immediately
#endif
}

void OpenUrlJobTest::runNativeExecutable_data()
{
    QTest::addColumn<bool>("withHandler");
    QTest::addColumn<bool>("handlerRetVal");

    QTest::newRow("no_handler") << false << false;
    QTest::newRow("handler_false") << true << false;
    QTest::newRow("handler_true") << true << true;
}

void OpenUrlJobTest::runNativeExecutable()
{
    QFETCH(bool, withHandler);
    QFETCH(bool, handlerRetVal);

#ifdef Q_OS_UNIX
    // Given an executable shell script that copies "src" to "dest" (we'll cheat with the mimetype to treat it like a native binary)
    QTemporaryDir tempDir;
    const QString dir = tempDir.path();
    createSrcFile(dir + QLatin1String("/src"));
    const QString scriptFile = dir + QLatin1String("/script.sh");
    QFile file(scriptFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\ncp src dest");
    file.close();
    // Note that it's missing executable permissions

    s_handler.m_calls.clear();
    s_handler.setRetVal(handlerRetVal);
    KIO::setDefaultUntrustedProgramHandler(withHandler ? &s_handler : nullptr);

    // When using OpenUrlJob to run the executable
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(scriptFile), QStringLiteral("application/x-executable"), this);
    job->setRunExecutables(true); // startProcess tests the case where this isn't set
    const bool success = job->exec();

    // Then --- it depends on what the user says via the handler
    if (!withHandler) {
        QVERIFY(!success);
        QCOMPARE(job->error(), KJob::UserDefinedError);
        QCOMPARE(job->errorString(), QStringLiteral("The program \"%1\" needs to have executable permission before it can be launched.").arg(scriptFile));
    } else {
        if (handlerRetVal) {
            QVERIFY(success);
            QTRY_VERIFY(QFileInfo::exists(dir + QLatin1String("/dest"))); // TRY because CommandLineLauncherJob finishes immediately
        } else {
            QVERIFY(!success);
            QCOMPARE(job->error(), KIO::ERR_USER_CANCELED);
        }
    }
#endif
}

void OpenUrlJobTest::launchExternalBrowser_data()
{
    QTest::addColumn<bool>("useBrowserApp");
    QTest::addColumn<bool>("useSchemeHandler");

    QTest::newRow("browserapp") << true << false;
    QTest::newRow("scheme_handler") << false << true;
}

void OpenUrlJobTest::launchExternalBrowser()
{
#ifdef Q_OS_UNIX
    QFETCH(bool, useBrowserApp);
    QFETCH(bool, useSchemeHandler);

    QTemporaryDir tempDir;
    const QString dir = tempDir.path();
    createSrcFile(dir + QLatin1String("/src"));
    const QString scriptFile = dir + QLatin1String("/browser.sh");
    QFile file(scriptFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("#!/bin/sh\necho $1 > `dirname $0`/destbrowser");
    file.close();
    QVERIFY(file.setPermissions(QFile::ExeUser | file.permissions()));

    QUrl remoteImage("http://example.org/image.jpg");
    if (useBrowserApp) {
        KConfigGroup(KSharedConfig::openConfig(), "General").writeEntry("BrowserApplication", QString(QLatin1Char('!') + scriptFile));
    } else if (useSchemeHandler) {
        remoteImage.setScheme("scheme");
    }

    // When using OpenUrlJob to run the script
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(remoteImage, this);

    // Then it works :-)
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QString dest;
    if (useBrowserApp) {
        dest = dir + QLatin1String("/destbrowser");
    } else if (useSchemeHandler) {
        dest = m_tempDir.path() + QLatin1String("/dest"); // see the .desktop file in writeApplicationDesktopFile
    }
    QTRY_VERIFY(QFileInfo::exists(dest)); // TRY because CommandLineLauncherJob finishes immediately
    QCOMPARE(readFile(dest), remoteImage.toString());

    // Restore settings
    KConfigGroup(KSharedConfig::openConfig(), "General").deleteEntry("BrowserApplication");
#endif
}

void OpenUrlJobTest::nonExistingFile()
{
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(QStringLiteral("/does/not/exist")), this);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_DOES_NOT_EXIST);
    QCOMPARE(job->errorString(), "The file or folder /does/not/exist does not exist.");
}

void OpenUrlJobTest::httpUrlWithKIO()
{
    // This tests the scanFileWithGet() code path
    const QUrl url(QStringLiteral("http://www.google.com/"));
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(url, this);
    job->setFollowRedirections(false);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    // Then the service should be executed (which writes to "dest")
    const QString dest = m_tempDir.path() + "/dest";
    QTRY_VERIFY2(QFile::exists(dest), qPrintable(dest));
    QCOMPARE(readFile(dest), url.toString());
}

void OpenUrlJobTest::ftpUrlWithKIO()
{
    // This is just to test the statFile() code at least a bit
    const QUrl url(QStringLiteral("ftp://localhost:2")); // unlikely that anything is running on that port
    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(url, this);
    QVERIFY(!job->exec());
    QCOMPARE(job->errorString(), "Could not connect to host localhost: Connection refused.");
}

void OpenUrlJobTest::takeOverAfterMimeTypeFound()
{
    // Given a local image file
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString srcFile = srcDir + QLatin1String("/image.jpg");
    createSrcFile(srcFile);

    KIO::OpenUrlJob *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(srcFile), this);
    QString foundMime = QStringLiteral("NONE");
    connect(job, &KIO::OpenUrlJob::mimeTypeFound, this, [&](const QString &mimeType) {
         foundMime = mimeType;
         job->kill();
    });
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KJob::KilledJobError);
    QCOMPARE(foundMime, "image/jpeg");
}

void OpenUrlJobTest::writeApplicationDesktopFile(const QString &filePath)
{
    KDesktopFile file(filePath);
    KConfigGroup group = file.desktopGroup();
    group.writeEntry("Name", "KRunUnittestService");
    group.writeEntry("MimeType", "text/plain;application/x-shellscript;x-scheme-handler/scheme");
    group.writeEntry("Type", "Application");
    group.writeEntry("Exec", QByteArray("echo %u > " + QFile::encodeName(m_tempDir.path()) + "/dest")); // not using %d because of remote urls
    QVERIFY(file.sync());
}
