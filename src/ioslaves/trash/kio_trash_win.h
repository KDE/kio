/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>
   Copyright (C) 2009 Christian Ehrlicher <ch.ehrlicher@gmx.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KIO_TRASH_H
#define KIO_TRASH_H

#include <kio/slavebase.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

namespace KIO { class Job; }

class TrashProtocol : public QObject, public KIO::SlaveBase
{
    Q_OBJECT
public:
    TrashProtocol( const QByteArray& protocol, const QByteArray &pool, const QByteArray &app );
    virtual ~TrashProtocol();
    virtual void stat( const QUrl& url );
    virtual void listDir( const QUrl& url );
    virtual void get( const QUrl& url );
    virtual void put( const QUrl& url, int , KIO::JobFlags flags );
    virtual void rename( const QUrl &, const QUrl &, KIO::JobFlags );
    virtual void copy( const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags );
    // TODO (maybe) chmod( const QUrl& url, int permissions );
    virtual void del( const QUrl &url, bool isfile );
    /**
     * Special actions: (first int in the byte array)
     * 1 : empty trash
     * 2 : migrate old (pre-kde-3.4) trash contents
     * 3 : restore a file to its original location. Args: QUrl trashURL.
     */
    virtual void special( const QByteArray & data );

    void updateRecycleBin();
private:
    typedef enum { Copy, Move } CopyOrMove;
    void copyOrMove( const QUrl& src, const QUrl& dest, bool overwrite, CopyOrMove action );
    void listRoot();
    void restore( const QUrl& trashURL, const QUrl &destURL );
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
