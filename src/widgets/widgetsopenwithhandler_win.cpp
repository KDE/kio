/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2008 Jarosław Staniek <staniek@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QDir>
#include <QWidget>

#include <qt_windows.h>

// TODO move to a shared lib
static int runDll(WId windowId, const QString &libraryName, const QByteArray &functionName,
                  const QString &arguments)
{
    HMODULE libHandle = LoadLibraryW((LPCWSTR)libraryName.utf16());
    if (!libHandle) {
        return 0;
    }
    typedef int (WINAPI * FunctionType)(HWND, HMODULE, LPCWSTR, int);
#ifdef _WIN32_WCE
    QString functionNamestr = QString(functionName);
    FunctionType function
        = (FunctionType)GetProcAddressW(libHandle, functionNamestr.utf16());
#else
    FunctionType function
        = (FunctionType)GetProcAddress(libHandle, functionName.constData());
#endif
    if (!function) {
        return 0;
    }
    int result = function((HWND)windowId, libHandle, (LPCWSTR)arguments.utf16(), SW_SHOW);
    FreeLibrary(libHandle);
    return result;
}

static int runDll(WId windowId, const QString &libraryName, const QByteArray &functionName,
                  const QByteArray &arguments)
{
    HMODULE libHandle = LoadLibraryW((LPCWSTR)libraryName.utf16());
    if (!libHandle) {
        return 0;
    }
    typedef int (WINAPI * FunctionType)(HWND, HMODULE, LPCSTR, int);
#ifdef _WIN32_WCE
    QString functionNamestr = QString(functionName);
    FunctionType function
        = (FunctionType)GetProcAddressW(libHandle, functionNamestr.utf16());
#else
    FunctionType function
        = (FunctionType)GetProcAddress(libHandle, functionName.constData());
#endif
    if (!function) {
        return 0;
    }
    int result = function((HWND)windowId, libHandle, (LPCSTR)arguments.constData(), SW_SHOW);
    FreeLibrary(libHandle);
    return result;
}

// TODO move to a shared lib
static int runDll(QWidget *parent, const QString &libraryName, const QByteArray &functionName,
                  const QString &arguments)
{
    return runDll(parent ? parent->winId() : 0, libraryName, functionName, arguments);
}

// Windows implementation using "OpenAs_RunDLL" entry
static bool displayNativeOpenWithDialog(const QList<QUrl> &lst, QWidget *window)
{
    QStringList fnames;
    for (const QUrl &url : lst) {
        fnames += url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : url.toString();
    }
    int result = runDll(window,
                        QLatin1String("shell32.dll"),
                        "OpenAs_RunDLLW",
                        fnames.join(QLatin1Char(' ')));
    return result == 0;
}
