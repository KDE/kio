/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>

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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef KIO_TRASH_H
#define KIO_TRASH_H

#include <kio/slavebase.h>
#include "trashimpl.h"
namespace KIO { class Job; }

class TrashProtocol : public QObject, public KIO::SlaveBase
{
    Q_OBJECT
public:
    TrashProtocol( const QCString& protocol, const QCString &pool, const QCString &app);
    virtual ~TrashProtocol();
    //virtual void get( const KURL& url );
    virtual void stat(const KURL& url);
    virtual void listDir(const KURL& url);
    virtual void put( const KURL& url, int , bool overwrite, bool );
    //virtual void mkdir( const KURL& url, int permissions );
    virtual void rename( const KURL &, const KURL &, bool );
    virtual void copy( const KURL &src, const KURL &dest, int permissions, bool overwrite );
    // TODO symlink( const QString& target, const KURL& dest, bool overwrite );
    // TODO chmod( const KURL& url, int permissions );
    virtual void del( const KURL &url, bool isfile );

private slots:
    void slotCopyResult( KIO::Job* job );

private:
    void createTopLevelDirEntry(KIO::UDSEntry& entry, const QString& name, const QString& url);
    bool createUDSEntry( const QString& physicalPath, const QString& fileName, const QString& url, KIO::UDSEntry& entry );
    void listRoot();
    static QString makeURL( int trashId, const QString& fileId, const QString& relativePath );
    static bool parseURL( const KURL& url, int& trashId, QString& fileId, QString& relativePath );
    friend class TestTrash;

    TrashImpl impl;
    QString m_userName;
    QString m_groupName;

    // During a copy
    int m_curTrashId;
    QString m_curFileId;
};

#endif
