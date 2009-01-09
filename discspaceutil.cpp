/*
   This file is part of the KDE project

   Copyright (C) 2008 Tobias Koenig <tokoe@kde.org>

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

#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

#include <kdiskfreespaceinfo.h>
#include <kdebug.h>
#include <kio/netaccess.h>
#include <kio/directorysizejob.h>

#include "discspaceutil.h"

DiscSpaceUtil::DiscSpaceUtil( const QString &directory )
    : mDirectory( directory ),
      mFullSize( 0 )
{
    calculateFullSize();
}

qulonglong DiscSpaceUtil::sizeOfPath( const QString &path )
{
    KIO::DirectorySizeJob* job = KIO::directorySize( KUrl(path) );
    job->setUiDelegate( 0 );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    return ok?job->totalSize():0;
}

double DiscSpaceUtil::usage( qulonglong additional ) const
{
    if ( mFullSize == 0 )
        return 0;

    qulonglong sum = sizeOfPath( mDirectory );
    sum += additional;

    return (((double)sum*100)/(double)mFullSize);
}

qulonglong DiscSpaceUtil::size() const
{
    return mFullSize;
}

QString DiscSpaceUtil::mountPoint() const
{
    return mMountPoint;
}

void DiscSpaceUtil::calculateFullSize()
{
    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo( mDirectory );
    if ( info.isValid() ) {
        mFullSize = info.size();
        mMountPoint = info.mountPoint();
    }
}
