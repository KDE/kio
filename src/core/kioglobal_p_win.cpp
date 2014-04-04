/* This file is part of the KDE libraries
Copyright (C) 2014 Alex Richardson <arichardson.kde@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License version 2 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.
*/
#include "kioglobal_p.h"

#include <QDebug>
#include <QFile>

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x600 //Vista is minimum supported windows version (we need CreateSymbolicLinkW)
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
}

KIOCORE_EXPORT bool KIOPrivate::changeOwnership(const QString& file, KUserId newOwner, KGroupId newGroup)
{
#pragma message("TODO")
    qWarning("KIOPrivate::changeOwnership: not implemented yet");
    return false;
}
