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

#include <thread>

namespace
{
// The event that sendTerminateSignal() sets for the worker process pid.
QString terminateEventName(qint64 pid)
{
    return QStringLiteral("Local\\org.kde.kio.worker.terminate.%1").arg(pid);
}

struct TerminateWatcher {
    HANDLE terminate = nullptr;
    HANDLE stop = nullptr;
    std::thread thread;
};

// Never destroyed: a worker can end with exit() while the thread still waits, and destroying a
// joinable std::thread would call std::terminate().
TerminateWatcher &terminateWatcher()
{
    static auto *watcher = new TerminateWatcher;
    return *watcher;
}
} // namespace

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
    // no error checking whether kill succeeded, Linux code also just sends a SIGTERM without checking
    // A worker has no window, so it waits for this event instead, see startTerminateWatcher().
    if (HANDLE event = OpenEventW(EVENT_MODIFY_STATE, FALSE, reinterpret_cast<LPCWSTR>(terminateEventName(pid).utf16()))) {
        SetEvent(event);
        CloseHandle(event);
    }
    HANDLE procHandle = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
    if (procHandle != INVALID_HANDLE_VALUE) {
        EnumWindows(&closeProcessCallback, (LPARAM)pid);
        CloseHandle(procHandle);
    }
}

KIOCORE_EXPORT void KIOPrivate::startTerminateWatcher(std::function<void()> onTerminate)
{
    TerminateWatcher &watcher = terminateWatcher();
    if (watcher.thread.joinable()) {
        return;
    }
    watcher.terminate = CreateEventW(nullptr, TRUE, FALSE, reinterpret_cast<LPCWSTR>(terminateEventName(GetCurrentProcessId()).utf16()));
    watcher.stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!watcher.terminate || !watcher.stop) {
        qWarning() << "Could not create the events to receive the termination of the worker:" << GetLastError();
        if (watcher.terminate) {
            CloseHandle(watcher.terminate);
        }
        if (watcher.stop) {
            CloseHandle(watcher.stop);
        }
        watcher.terminate = watcher.stop = nullptr;
        return;
    }
    watcher.thread = std::thread([terminate = watcher.terminate, stop = watcher.stop, onTerminate = std::move(onTerminate)]() {
        const HANDLE events[] = {terminate, stop};
        if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0) {
            return;
        }
        onTerminate();
        // As alarm(5) after SIGTERM on UNIX, a worker that has not stopped by then is ended.
        if (WaitForSingleObject(stop, 5000) == WAIT_TIMEOUT) {
            TerminateProcess(GetCurrentProcess(), 255);
        }
    });
}

KIOCORE_EXPORT void KIOPrivate::stopTerminateWatcher()
{
    TerminateWatcher &watcher = terminateWatcher();
    if (!watcher.thread.joinable()) {
        return;
    }
    SetEvent(watcher.stop);
    watcher.thread.join();
    CloseHandle(watcher.terminate);
    CloseHandle(watcher.stop);
    watcher.terminate = watcher.stop = nullptr;
}

KIOCORE_EXPORT bool KIOPrivate::createSymlink(const QString &source, const QString &destination, KIOPrivate::SymlinkType type)
{
#if _WIN32_WINNT >= 0x600
    DWORD flag;
    if (type == KIOPrivate::DirectorySymlink) {
        flag = SYMBOLIC_LINK_FLAG_DIRECTORY;
    } else if (type == KIOPrivate::FileSymlink) {
        flag = 0;
    } else {
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

KIOCORE_EXPORT int kio_windows_lstat(const char *path, QT_STATBUF *buffer)
{
    int result = QT_STAT(path, buffer);
    if (result != 0) {
        return result;
    }
    const QString pathStr = QFile::decodeName(path);
    // QFileInfo currently only checks for .lnk file in isSymlink() -> also check native win32 symlinks
    const DWORD fileAttrs = GetFileAttributesW((LPCWSTR)pathStr.utf16());
    if (fileAttrs != INVALID_FILE_ATTRIBUTES && (fileAttrs & FILE_ATTRIBUTE_REPARSE_POINT) || QFileInfo(pathStr).isSymLink()) {
        buffer->st_mode |= QT_STAT_LNK;
    }
    return result;
}

KIOCORE_EXPORT bool KIOPrivate::changeOwnership(const QString &file, KUserId newOwner, KGroupId newGroup)
{
#pragma message("TODO")
    qWarning("KIOPrivate::changeOwnership: not implemented yet");
    return false;
}
