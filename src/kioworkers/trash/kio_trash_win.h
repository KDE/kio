/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2009 Christian Ehrlicher <ch.ehrlicher@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_TRASH_H
#define KIO_TRASH_H

#define NOMINMAX

#include <kio/workerbase.h>

#include <windows.h> // Must be included before shallapi.h, otherwise it fails to build on windows

#include <shellapi.h>
#include <shlobj.h>

#include <KConfig>

namespace KIO
{
class Job;
}

class TrashProtocol : public QObject, public KIO::WorkerBase
{
    Q_OBJECT
public:
    TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    virtual ~TrashProtocol();
    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult put(const QUrl &url, int, KIO::JobFlags flags) override;
    KIO::WorkerResult rename(const QUrl &, const QUrl &, KIO::JobFlags) override;
    KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) override;
    // TODO (maybe) chmod( const QUrl& url, int permissions );
    KIO::WorkerResult del(const QUrl &url, bool isfile) override;
    /**
     * Special actions: (first int in the byte array)
     * 1 : empty trash
     * 2 : migrate old (pre-kde-3.4) trash contents
     * 3 : restore a file to its original location. Args: QUrl trashURL.
     */
    KIO::WorkerResult special(const QByteArray &data) override;

    void updateRecycleBin();

private:
    typedef enum {
        Copy,
        Move
    } CopyOrMove;
    [[nodiscard]] KIO::WorkerResult copyOrMove(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action);
    [[nodiscard]] KIO::WorkerResult listRoot();
    [[nodiscard]] KIO::WorkerResult restore(const QUrl &trashURL, const QUrl &destURL);
    [[nodiscard]] KIO::WorkerResult clearTrash();

    [[nodiscard]] KIO::WorkerResult doFileOp(const QUrl &url, UINT wFunc, FILEOP_FLAGS fFlags);
    [[nodiscard]] KIO::WorkerResult translateError(HRESULT retValue);

    KConfig m_config;
    HWND m_notificationWindow;
    IShellFolder2 *m_isfTrashFolder;
    LPMALLOC m_pMalloc;
    ULONG m_hNotifyRBin;
};

#endif
