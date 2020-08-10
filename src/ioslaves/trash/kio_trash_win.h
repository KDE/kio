/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2009 Christian Ehrlicher <ch.ehrlicher@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_TRASH_H
#define KIO_TRASH_H

#define NOMINMAX

#include <kio/slavebase.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <KConfig>

namespace KIO
{
class Job;
}

class TrashProtocol : public QObject, public KIO::SlaveBase
{
    Q_OBJECT
public:
    TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    virtual ~TrashProtocol();
    virtual void stat(const QUrl &url);
    virtual void listDir(const QUrl &url);
    virtual void get(const QUrl &url);
    virtual void put(const QUrl &url, int, KIO::JobFlags flags);
    virtual void rename(const QUrl &, const QUrl &, KIO::JobFlags);
    virtual void copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags);
    // TODO (maybe) chmod( const QUrl& url, int permissions );
    virtual void del(const QUrl &url, bool isfile);
    /**
     * Special actions: (first int in the byte array)
     * 1 : empty trash
     * 2 : migrate old (pre-kde-3.4) trash contents
     * 3 : restore a file to its original location. Args: QUrl trashURL.
     */
    virtual void special(const QByteArray &data);

    void updateRecycleBin();
private:
    typedef enum { Copy, Move } CopyOrMove;
    void copyOrMove(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action);
    void listRoot();
    void restore(const QUrl &trashURL, const QUrl &destURL);
    void clearTrash();

    bool doFileOp(const QUrl &url, UINT wFunc, FILEOP_FLAGS fFlags);
    bool translateError(HRESULT retValue);

    KConfig m_config;
    HWND m_notificationWindow;
    IShellFolder2 *m_isfTrashFolder;
    LPMALLOC m_pMalloc;
    ULONG m_hNotifyRBin;
};

#endif
