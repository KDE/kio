/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999-2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 1999 Mario Weilguni <mweilguni@sime.com>
    SPDX-FileCopyrightText: 2001 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <cerrno>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <QFile>
#include <QLibrary>
#include <QPluginLoader>
#include <QString>

#ifdef Q_OS_WIN
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

#include <process.h>
#include <qt_windows.h>
#endif

#ifndef Q_OS_WIN
#include <unistd.h>

/* These are to link libkio even if 'smart' linker is used */
#include <kio/authinfo.h>
extern "C" KIO::AuthInfo *_kioworker_init_kio()
{
    return new KIO::AuthInfo();
}
#endif

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "Usage: kioworker <worker-lib> <protocol> <klauncher-socket> <app-socket>\n\nThis program is part of KDE.\n");
        return 1;
    }

    setlocale(LC_ALL, "");
    QString libname = QFile::decodeName(argv[1]);

    if (libname.isEmpty()) {
        fprintf(stderr, "library path is empty.\n");
        return 1;
    }

    // Use QPluginLoader to locate the library when using a relative path
    // But we need to use QLibrary to actually load it, because of resolve()!
    QString libpath = QPluginLoader(libname).fileName();
    if (libpath.isEmpty()) {
        fprintf(stderr, "could not locate %s, check QT_PLUGIN_PATH\n", qPrintable(libname));
        return 1;
    }

    QLibrary lib(libpath);
    if (!lib.load()) {
        fprintf(stderr, "could not open %s: %s\n", qPrintable(libname), qPrintable(lib.errorString()));
        return 1;
    }

    QFunctionPointer sym = lib.resolve("kdemain");
    if (!sym) {
        fprintf(stderr, "Could not find kdemain: %s\n", qPrintable(lib.errorString()));
        return 1;
    }

    const QByteArray workerDebugWait = qgetenv("KIOWORKER_DEBUG_WAIT");

#ifdef Q_OS_WIN
    // enter debugger in case debugging is activated
    if (workerDebugWait == "all" || workerDebugWait == argv[2]) {
#ifdef Q_CC_MSVC
        // msvc debugger or windbg supports jit debugging, the latter requires setting up windbg jit with windbg -i
        DebugBreak();
#else
        // gdb does not support win32 jit debug support, so implement it by ourself
        WCHAR buf[1024];
        GetModuleFileName(NULL, buf, 1024);
        QStringList params;
        params << QString::fromUtf16(reinterpret_cast<const char16_t *>(buf));
        params << QString::number(GetCurrentProcessId());
        const QString gdbExec = QStandardPaths::findExecutable(QStringLiteral("gdb"));
        if (gdbExec.isEmpty()) {
            fprintf(stderr, "Could not find 'gdb' executable in PATH\n");
            return 1;
        }
        QProcess::startDetached(gdbExec, params);
        Sleep(1000);
#endif
    }
#if defined(Q_CC_MSVC)
    else {
        QString workerDebugPopup(QString::fromLocal8Bit(qgetenv("KIOWORKER_DEBUG_POPUP")));
        if (workerDebugPopup == QLatin1String("all") || workerDebugPopup == QString::fromLocal8Bit(argv[2])) {
            // A workaround for OSes where DebugBreak() does not work in administrative mode (actually Vista with msvc 2k5)
            // - display a native message box so developer can attach the debugger to the KIO worker process and click OK.
            MessageBoxA(
                NULL,
                QStringLiteral("Please attach the debugger to process #%1 (%2)").arg(getpid()).arg(QString::fromLocal8Bit(argv[0])).toLatin1().constData(),
                QStringLiteral("\"%1\" KIO Worker Debugging").arg(QString::fromLocal8Bit(argv[2])).toLatin1().constData(),
                MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
        }
    }
#endif
#endif // Q_OS_WIN

#if defined(Q_OS_UNIX)
    // Enter debugger in case debugging is activated
    if (workerDebugWait == "all" || workerDebugWait == argv[2]) {
        const pid_t pid = getpid();
        fprintf(stderr,
                "kioworker: Suspending process to debug io worker(s): %s\n"
                "kioworker: 'gdb kioworker %d' to debug\n"
                "kioworker: 'kill -SIGCONT %d' to continue\n",
                workerDebugWait.constData(),
                pid,
                pid);

        kill(pid, SIGSTOP);
    }
#endif

    int (*func)(int, char *[]) = (int (*)(int, char *[]))sym;

    // We need argv[0] to remain /path/to/kioworker
    // so that applicationDirPath() is correct on non-Linux (no /proc)
    // and we want to skip argv[1] so the kioworker exe is transparent to kdemain.
    const int newArgc = argc - 1;
    QVarLengthArray<char *, 5> newArgv(newArgc);
    newArgv[0] = argv[0];
    for (int i = 1; i < newArgc; ++i) {
        newArgv[i] = argv[i + 1];
    }

    return func(newArgc, newArgv.data()); /* Launch! */
}
