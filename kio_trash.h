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

class TrashProtocol : public KIO::SlaveBase
{
public:
    TrashProtocol( const QCString& protocol, const QCString &pool, const QCString &app);
    virtual ~TrashProtocol();
    virtual void get( const KURL& url );
    virtual void stat(const KURL& url);
    virtual void listDir(const KURL& url);
    virtual void put( const KURL& url, int , bool overwrite, bool );
    virtual void mkdir( const KURL& url, int permissions );
    virtual void rename(KURL const &, KURL const &, bool);
    // TODO copy()
    // TODO symlink()
    // TODO chmod()
    // TODO del()

private:
    void createTopLevelDirEntry(KIO::UDSEntry& entry, const QString& name, const QString& url);
    void listRoot();

    TrashImpl impl;
    QString m_userName;
    QString m_groupName;
};

#endif
