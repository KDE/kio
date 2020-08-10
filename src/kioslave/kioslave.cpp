/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999-2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 1999 Mario Weilguni <mweilguni@sime.com>
    SPDX-FileCopyrightText: 2001 Lubos Lunak <l.lunak@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <locale.h>

#include <QString>
#include <QLibrary>
#include <QFile>

#include <KPluginLoader>

#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
#define USE_KPROCESS_FOR_KIOSLAVES
#endif

#ifdef USE_KPROCESS_FOR_KIOSLAVES
#include <QProcess>
#include <QStringList>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <process.h>
#endif
#endif

#ifndef Q_OS_WIN
/* These are to link libkio even if 'smart' linker is used */
#include <kio/authinfo.h>
extern "C" KIO::AuthInfo *_kioslave_init_kio()
{
    return new KIO::AuthInfo();
}
#endif

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "Usage: kioslave5 <slave-lib> <protocol> <klauncher-socket> <app-socket>\n\nThis program is part of KDE.\n");
        return 1;
    }
#ifndef _WIN32_WCE
    setlocale(LC_ALL, "");
#endif
    QString libname = QFile::decodeName(argv[1]);

    if (libname.isEmpty()) {
        fprintf(stderr, "library path is empty.\n");
        return 1;
    }

    // Use KPluginLoader to locate the library when using a relative path
    // But we need to use QLibrary to actually load it, because of resolve()!
    QString libpath = KPluginLoader::findPlugin(libname);
    if (libpath.isEmpty()) {
        fprintf(stderr, "could not locate %s, check QT_PLUGIN_PATH\n", qPrintable(libname));
        return 1;
    }

    QLibrary lib(libpath);
    if (!lib.load()) {
        fprintf(stderr, "could not open %s: %s\n", qPrintable(libname),
                qPrintable(lib.errorString()));
        return 1;
    }

    QFunctionPointer sym = lib.resolve("kdemain");
    if (!sym) {
        fprintf(stderr, "Could not find kdemain: %s\n", qPrintable(lib.errorString()));
        return 1;
    }

#ifdef Q_OS_WIN
    // enter debugger in case debugging is activated
    QString slaveDebugWait(QString::fromLocal8Bit(qgetenv("KDE_SLAVE_DEBUG_WAIT")));
    if (slaveDebugWait == QLatin1String("all") || slaveDebugWait == QString::fromLocal8Bit(argv[2])) {
# ifdef Q_CC_MSVC
        // msvc debugger or windbg supports jit debugging, the latter requires setting up windbg jit with windbg -i
        DebugBreak();
# else
        // gdb does not support win32 jit debug support, so implement it by ourself
        WCHAR buf[1024];
        GetModuleFileName(NULL, buf, 1024);
        QStringList params;
        params << QString::fromUtf16((const unsigned short *)buf);
        params << QString::number(GetCurrentProcessId());
        QProcess::startDetached(QStringLiteral("gdb"), params);
        Sleep(1000);
# endif
    }
# if defined(Q_CC_MSVC) && !defined(_WIN32_WCE)
    else {
        QString slaveDebugPopup(QString::fromLocal8Bit(qgetenv("KDE_SLAVE_DEBUG_POPUP")));
        if (slaveDebugPopup == QLatin1String("all") || slaveDebugPopup == QString::fromLocal8Bit(argv[2])) {
            // A workaround for OSes where DebugBreak() does not work in administrative mode (actually Vista with msvc 2k5)
            // - display a native message box so developer can attach the debugger to the KIO slave process and click OK.
            MessageBoxA(NULL,
                        QStringLiteral("Please attach the debugger to process #%1 (%2)").arg(getpid()).arg(QString::fromLocal8Bit(argv[0])).toLatin1().constData(),
                        QStringLiteral("\"%1\" KIO Slave Debugging").arg(QString::fromLocal8Bit(argv[2])).toLatin1().constData(),
                        MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
        }
    }
# endif
#endif // Q_OS_WIN

    int (*func)(int, char *[]) = (int (*)(int, char *[])) sym;

    // We need argv[0] to remain /path/to/kioslave5
    // so that applicationDirPath() is correct on non-Linux (no /proc)
    // and we want to skip argv[1] so the kioslave5 exe is transparent to kdemain.
    const int newArgc = argc - 1;
    QVarLengthArray<char*, 5> newArgv(newArgc);
    newArgv[0] = argv[0];
    for (int i = 1; i < newArgc; ++i) {
        newArgv[i] = argv[i+1];
    }

    return func(newArgc, newArgv.data()); /* Launch! */
}
