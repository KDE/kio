#ifndef __help_h__
#define __help_h__

/* This file is part of the KDE libraries
   Copyright (C) 2000 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
   Copyright (C) 2001 Stephan Kulow <coolo@kde.org>
   Copyright (C) 2003 Cornelius Schumacher <schumacher@kde.org>

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

#include <kio/global.h>
#include <kio/slavebase.h>

#include <QtCore/QString>

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <unistd.h>

class HelpProtocol : public KIO::SlaveBase
{
public:

    HelpProtocol( bool ghelp, const QByteArray &pool, const QByteArray &app);
    virtual ~HelpProtocol() { }

    virtual void get( const QUrl& url );

    virtual void mimetype( const QUrl& url );

private:

    QString langLookup(const QString &fname);
    void emitFile( const QUrl &url );
    void get_file(const QString& path);
    QString lookupFile(const QString &fname, const QString &query,
                       bool &redirect);

    void unicodeError( const QString &t );

    QString mParsed;
    bool mGhelp;
};


#endif
