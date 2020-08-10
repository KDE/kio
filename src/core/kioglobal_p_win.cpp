/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Alex Richardson <arichardson.kde@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#include "kioglobal_p.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include <qt_windows.h>

KIOCORE_EXPORT bool KIOPrivate::isProcessAlive(qint64 pid) {
    HANDLE procHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    bool alive = false;
    if (procHandle != INVALID_HANDLE_VALUE) {
        DWORD exitCode;
        if (GetExitCodeProcess(procHandle, &exitCode)) {
            alive = exitCode == STILL_ACTIVE;
        }
        CloseHandle(procHandle);
    }
    return alive;
}

// A callback to shutdown cleanly (no forced kill)
BOOL CALLBACK closeProcessCallback(HWND hwnd, LPARAM lParam)
{
    DWORD id;
    GetWindowThreadProcessId(hwnd, &id);
    if (id == (DWORD)lParam) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

KIOCORE_EXPORT void KIOPrivate::sendTerminateSignal(qint64 pid)
{
    //no error checking whether kill succeeded, Linux code also just sends a SIGTERM without checking
    HANDLE procHandle = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
    if (procHandle != INVALID_HANDLE_VALUE) {
        EnumWindows(&closeProcessCallback, (LPARAM)pid);
        CloseHandle(procHandle);
    }
}

KIOCORE_EXPORT bool KIOPrivate::createSymlink(const QString &source, const QString &destination, KIOPrivate::SymlinkType type)
{
#if _WIN32_WINNT >= 0x600
    DWORD flag;
    if (type == KIOPrivate::DirectorySymlink) {
        flag = SYMBOLIC_LINK_FLAG_DIRECTORY;
    } else if (type == KIOPrivate::FileSymlink) {
        flag = 0;
    }
    else {
        // Guess the type of the symlink based on the source path
        // If the source is a directory we set the flag SYMBOLIC_LINK_FLAG_DIRECTORY, for files
        // and non-existent paths we create a symlink to a file
        DWORD sourceAttrs = GetFileAttributesW((LPCWSTR)source.utf16());
        if (sourceAttrs != INVALID_FILE_ATTRIBUTES && (sourceAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
            flag = SYMBOLIC_LINK_FLAG_DIRECTORY;
        } else {
            flag = 0;
        }
    }
    bool ok = CreateSymbolicLinkW((LPCWSTR)destination.utf16(), (LPCWSTR)source.utf16(), flag);
    if (!ok) {
        // create a .lnk file
        ok = QFile::link(source, destination);
    }
    return ok;
#else
    qWarning("KIOPrivate::createSymlink: not implemented");
    return false;
#endif
}

KIOCORE_EXPORT int kio_windows_lstat(const char* path, QT_STATBUF* buffer)
{
    int result = QT_STAT(path, buffer);
    if (result != 0) {
        return result;
    }
    const QString pathStr = QFile::decodeName(path);
    // QFileInfo currently only checks for .lnk file in isSymlink() -> also check native win32 symlinks
    const DWORD fileAttrs = GetFileAttributesW((LPCWSTR)pathStr.utf16());
    if (fileAttrs != INVALID_FILE_ATTRIBUTES && (fileAttrs & FILE_ATTRIBUTE_REPARSE_POINT)
        || QFileInfo(pathStr).isSymLink()) {
        buffer->st_mode |= QT_STAT_LNK;
    }
    return result;
}

KIOCORE_EXPORT bool KIOPrivate::changeOwnership(const QString& file, KUserId newOwner, KGroupId newGroup)
{
#pragma message("TODO")
    qWarning("KIOPrivate::changeOwnership: not implemented yet");
    return false;
}
