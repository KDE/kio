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

#include "trashimpl.h"
#include <klocale.h>
#include <klargefile.h>
#include <kio/global.h>
#include <kio/renamedlg.h>
#include <kdebug.h>

#include <qfile.h>
#include <qdir.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>

TrashImpl::TrashImpl() :
    m_lastErrorCode( 0 ),
    m_initStatus( InitToBeDone ),
    m_lastId( 0 )
{
}

/**
 * Test if a directory exists, create otherwise
 * @param _name full path of the directory
 * @return true if the dir was created or existed already
 */
bool TrashImpl::testDir( const QString &_name )
{
  DIR *dp = opendir( QFile::encodeName(_name) );
  if ( dp == NULL )
  {
    QString name = _name;
    if ( name.endsWith( "/" ) )
      name.truncate( name.length() - 1 );
    QCString path = QFile::encodeName(name);

    bool ok = ::mkdir( path, S_IRWXU ) == 0;
    if ( !ok && errno == EEXIST ) {
#if 0 // this would require to use SlaveBase's method to ask the question
        //int ret = KMessageBox::warningYesNo( 0, i18n("%1 is a file, but KDE needs it to be a directory. Move it to %2.orig and create directory?").arg(name).arg(name) );
        //if ( ret == KMessageBox::Yes ) {
#endif
            if ( ::rename( path, path + ".orig" ) == 0 ) {
                ok = ::mkdir( path, S_IRWXU ) == 0;
            } else { // foo.orig existed already. How likely is that?
                ok = false;
            }
            if ( !ok ) {
                error( KIO::ERR_DIR_ALREADY_EXIST, name );
                return false;
            }
#if 0
        //} else {
        //    return false;
        //}
#endif
    }
    if ( !ok )
    {
        //KMessageBox::sorry( 0, i18n( "Couldn't create directory %1. Check for permissions or reconfigure the desktop to use another path." ).arg( m ) );
        error( KIO::ERR_COULD_NOT_MKDIR, name );
        return false;
    }
  }
  else // exists already
  {
    closedir( dp );
  }
  return true;
}

bool TrashImpl::init()
{
    if ( m_initStatus == InitOK )
        return true;
    if ( m_initStatus == InitError )
        return false;

    // Check $HOME/.Trash/{info,files}/
    m_initStatus = InitError;
    QString trashDir = QDir::homeDirPath() + "/.Trash";
    if ( !testDir( trashDir ) )
        return false;
    if ( !testDir( trashDir + "/info" ) )
        return false;
    if ( !testDir( trashDir + "/files" ) )
        return false;
    m_trashDirectories.insert( 0, trashDir );
    m_initStatus = InitOK;
    return true;
}

bool TrashImpl::add( const QString& origPath )
{
    kdDebug() << k_funcinfo << origPath << endl;
    // Check source
    QCString _src( QFile::encodeName(origPath) );
    KDE_struct_stat buff_src;
    if ( KDE_stat( _src.data(), &buff_src ) == -1 ) {
        if ( errno == EACCES )
           error( KIO::ERR_ACCESS_DENIED, origPath );
        else
           error( KIO::ERR_DOES_NOT_EXIST, origPath );
        return false;
    }

    // Choose destination trash
    int trashId = findTrashDirectory( origPath );
    if ( trashId < 0 )
        return false; // ### error() needed?
    QString trashPath = trashDirectoryPath( trashId );

    // Grab original filename
    KURL url;
    url.setPath( origPath );
    QString origFileName = url.fileName();

    // Make destination file in info/
    url.setPath( trashPath );
    url.addPath( "info" );
    KURL baseDirectory( url );
    url.addPath( origFileName );
    // Here we need to use O_EXCL to avoid race conditions with other kioslave processes
    int fd = 0;
    do {
        kdDebug() << k_funcinfo << "trying to create " << url.path()  << endl;
        fd = ::open( QFile::encodeName( url.path() ), O_WRONLY | O_CREAT | O_EXCL, 0600 );
        if ( fd < 0 ) {
            if ( errno == EEXIST ) {
                url.setFileName( KIO::RenameDlg::suggestName( baseDirectory, url.fileName() ) );
                // and try again on the next iteration
            } else {
                error( KIO::ERR_COULD_NOT_WRITE, url.path() );
                return false;
            }
        }
    } while ( fd < 0 );
    QString infoPath = url.path();
    QString fileId = url.fileName();

    FILE* file = ::fdopen( fd, "w" );
    if ( !file ) { // can't see how this would happen
        error( KIO::ERR_COULD_NOT_WRITE, infoPath );
        return false;
    }

    // Contents of the info file
    QCString info = QFile::encodeName( origPath );
    info += "\n";
    info += QDateTime::currentDateTime().toString( Qt::ISODate ).local8Bit();
    size_t sz = info.size() - 1; // avoid trailing 0 from QCString

    size_t written = ::fwrite(info.data(), 1, sz, file);
    if ( written != sz ) {
        ::fclose( file );
        ::unlink( QFile::encodeName( infoPath ) );
        error( KIO::ERR_DISK_FULL, infoPath );
        return false;
    }

    ::fclose( file );

    url.setPath( trashPath );
    url.addPath( "files" );
    url.addPath( fileId );
    kdDebug() << k_funcinfo << "moving to " << url.path() << endl;

    if ( !tryRename( _src, QFile::encodeName( url.path() ) ) ) {
        if ( m_lastErrorCode == KIO::ERR_UNSUPPORTED_ACTION ) {
            // OK that's not really an error, the file is simply on another partition
            // We *keep* the info file around, now that it's done.
            // We'll just add the file in place when we get it via put().
        } else { // real error, delete info file
            ::unlink( QFile::encodeName( infoPath ) );
        }
        return false;
    }

    return true;
}

bool TrashImpl::tryRename( const char* src, const char* dest )
{
    if ( ::rename( src, dest ) != 0 ) {
        if (errno == EXDEV) {
            error( KIO::ERR_UNSUPPORTED_ACTION, QString::fromLatin1("rename"));
        } else {
            if (( errno == EACCES ) || (errno == EPERM)) {
                error( KIO::ERR_ACCESS_DENIED, QFile::decodeName( dest ) );
            } else if (errno == EROFS) { // The file is on a read-only filesystem
                error( KIO::ERR_CANNOT_DELETE, QFile::decodeName( src ) );
            } else {
                error( KIO::ERR_CANNOT_RENAME, QFile::decodeName( src ) );
            }
        }
        return false;
    }
    return true;
}

bool TrashImpl::del( int trashId, int fileId )
{
    // TODO
}

bool TrashImpl::restore( int trashId, int fileId )
{
    // TODO
}

bool TrashImpl::emptyTrash()
{
    // TODO
    return false;
}

QValueList<TrashImpl::TrashedFileInfo> TrashImpl::list()
{
    QValueList<TrashedFileInfo> lst;
    // TODO
    return lst;
}

bool TrashImpl::infoForFile( TrashedFileInfo& info )
{
    // TODO
    return false;
}


int TrashImpl::findTrashDirectory( const QString& origPath )
{
    // TODO implement the real algorithm
    return 0;
}

bool TrashImpl::initTrashDirectory( const QString& origPath )
{
    // TODO
    return true;
}

void TrashImpl::error( int e, QString s )
{
    kdDebug() << k_funcinfo << e << " " << s << endl;
    m_lastErrorCode = e;
    m_lastErrorMessage = s;
}
