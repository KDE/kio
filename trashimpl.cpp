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
#include <kio/job.h>
#include <kdebug.h>
#include <kurl.h>
#include <kglobalsettings.h>

#include <dcopref.h>

#include <qapplication.h>
#include <qeventloop.h>
#include <qfile.h>
#include <qdir.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>

TrashImpl::TrashImpl() :
    QObject(),
    m_lastErrorCode( 0 ),
    m_initStatus( InitToBeDone ),
    m_lastId( 0 ),
    // not using kio_trashrc since KIO uses that one already for kio_trash
    // so better have a separate one, for faster parsing by e.g. kmimetype.cpp
    m_config( "trashrc" )
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
        //KMessageBox::sorry( 0, i18n( "Couldn't create directory %1. Check for permissions or reconfigure the desktop to use another path." ).arg( name ) );
        error( KIO::ERR_COULD_NOT_MKDIR, name );
        return false;
    } else {
        kdDebug() << name << " created." << endl;
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
    // see also kdesktop/init.cc for first time initialization
    const QString trashDir = QDir::homeDirPath() + "/.Trash";
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

void TrashImpl::migrateOldTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString oldTrashDir = KGlobalSettings::trashPath();
    const QStrList entries = listDir( oldTrashDir );
    bool allOK = true;
    QStrListIterator entryIt( entries );
    for (; entryIt.current(); ++entryIt) {
        QString srcPath = QFile::decodeName( *entryIt );
        if ( srcPath == "." || srcPath == ".." || srcPath == ".directory" )
            continue;
        srcPath.prepend( oldTrashDir ); // make absolute
        int trashId;
        QString fileId;
        if ( !createInfo( srcPath, trashId, fileId ) ) {
            kdWarning() << "Trash migration: failed to create info for " << srcPath << endl;
            allOK = false;
        } else {
            bool ok = moveToTrash( srcPath, trashId, fileId );
            if ( !ok ) {
                (void)deleteInfo( trashId, fileId );
                kdWarning() << "Trash migration: failed to create info for " << srcPath << endl;
                allOK = false;
            } else {
                kdDebug() << "Trash migration: moved " << srcPath << endl;
            }
        }
    }
    if ( allOK ) {
        // We need to remove the old one, otherwise the desktop will have two trashcans...
        kdDebug() << "Trash migration: all OK, removing old trash directory" << endl;
        synchronousDel( oldTrashDir );
    }
}

bool TrashImpl::createInfo( const QString& origPath, int& trashId, QString& fileId )
{
    //kdDebug() << k_funcinfo << origPath << endl;
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
    trashId = findTrashDirectory( origPath );
    if ( trashId < 0 )
        return false; // ### error() needed?

    // Grab original filename
    KURL url;
    url.setPath( origPath );
    const QString origFileName = url.fileName();

    // Make destination file in info/
    url.setPath( infoPath( trashId, origFileName ) ); // we first try with origFileName
    KURL baseDirectory;
    baseDirectory.setPath( url.directory() );
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
    const QString infoPath = url.path();
    fileId = url.fileName();
    Q_ASSERT( fileId.endsWith( ".trashinfo" ) );
    fileId.truncate( fileId.length() - 10 ); // remove .trashinfo from fileId

    FILE* file = ::fdopen( fd, "w" );
    if ( !file ) { // can't see how this would happen
        error( KIO::ERR_COULD_NOT_WRITE, infoPath );
        return false;
    }

    // Contents of the info file. We could use KSimpleConfig, but that would
    // mean closing and reopening fd, i.e. opening a race condition...
    QCString info = "[Trash Info]\n";
    info += "Path=";
    info += QFile::encodeName( origPath );
    info += "\n";
    info += "DeletionDate=";
    info += QDateTime::currentDateTime().toString( Qt::ISODate ).local8Bit();
    info += "\n";
    size_t sz = info.size() - 1; // avoid trailing 0 from QCString

    size_t written = ::fwrite(info.data(), 1, sz, file);
    if ( written != sz ) {
        ::fclose( file );
        ::unlink( QFile::encodeName( infoPath ) );
        error( KIO::ERR_DISK_FULL, infoPath );
        return false;
    }

    ::fclose( file );

    kdDebug() << k_funcinfo << "info file created in trashId=" << trashId << " : " << fileId << endl;
    return true;
}

QString TrashImpl::infoPath( int trashId, const QString& fileId ) const
{
    QString trashPath = trashDirectoryPath( trashId );
    trashPath += "/info/";
    trashPath += fileId;
    trashPath += ".trashinfo";
    return trashPath;
}

QString TrashImpl::filesPath( int trashId, const QString& fileId ) const
{
    QString trashPath = trashDirectoryPath( trashId );
    trashPath += "/files/";
    trashPath += fileId;
    return trashPath;
}

bool TrashImpl::deleteInfo( int trashId, const QString& fileId )
{
    return ( ::unlink( QFile::encodeName( infoPath( trashId, fileId ) ) ) == 0 );
}

bool TrashImpl::moveToTrash( const QString& origPath, int trashId, const QString& fileId )
{
    kdDebug() << k_funcinfo << endl;
    const QString dest = filesPath( trashId, fileId );
    if ( !move( origPath, dest ) )
        return false;
    fileAdded();
    return true;
}

bool TrashImpl::moveFromTrash( const QString& dest, int trashId, const QString& fileId, const QString& relativePath )
{
    QString src = filesPath( trashId, fileId );
    if ( !relativePath.isEmpty() ) {
        src += '/';
        src += relativePath;
    }
    if ( !move( src, dest ) )
        return false;
    fileRemoved();
    return true;
}

bool TrashImpl::move( const QString& src, const QString& dest )
{
    if ( directRename( src, dest ) )
        return true;
    if ( m_lastErrorCode != KIO::ERR_UNSUPPORTED_ACTION )
        return false;
    KURL urlSrc, urlDest;
    urlSrc.setPath( src );
    urlDest.setPath( dest );
    kdDebug() << k_funcinfo << urlSrc << " -> " << urlDest << endl;
    KIO::CopyJob* job = KIO::moveAs( urlSrc, urlDest, false );
    connect( job, SIGNAL( result(KIO::Job *) ),
             this, SLOT( jobFinished(KIO::Job *) ) );
    qApp->eventLoop()->enterLoop();

    return m_lastErrorCode == 0;
}

void TrashImpl::jobFinished(KIO::Job* job)
{
    error( job->error(), job->errorText() );
    qApp->eventLoop()->exitLoop();
}

bool TrashImpl::copyToTrash( const QString& origPath, int trashId, const QString& fileId )
{
    kdDebug() << k_funcinfo << endl;
    const QString dest = filesPath( trashId, fileId );
    if ( !copy( origPath, dest ) )
        return false;
    fileAdded();
    return true;
}

bool TrashImpl::copyFromTrash( const QString& dest, int trashId, const QString& fileId, const QString& relativePath )
{
    QString src = filesPath( trashId, fileId );
    if ( !relativePath.isEmpty() ) {
        src += '/';
        src += relativePath;
    }
    return copy( src, dest );
}

bool TrashImpl::copy( const QString& src, const QString& dest )
{
    // kio_file's copy() method is quite complex (in order to be fast), let's just call it...
    KURL urlSrc;
    urlSrc.setPath( src );
    KURL urlDest;
    urlDest.setPath( dest );
    kdDebug() << k_funcinfo << "copying " << src << " to " << dest << endl;
    KIO::Job* job = KIO::copyAs( urlSrc, urlDest, false );
    connect( job, SIGNAL( result( KIO::Job* ) ),
             this, SLOT( jobFinished( KIO::Job* ) ) );
    qApp->eventLoop()->enterLoop();

    return m_lastErrorCode == 0;
}

bool TrashImpl::directRename( const QString& src, const QString& dest )
{
    kdDebug() << k_funcinfo << src << " -> " << dest << endl;
    if ( ::rename( QFile::encodeName( src ), QFile::encodeName( dest ) ) != 0 ) {
        if (errno == EXDEV) {
            error( KIO::ERR_UNSUPPORTED_ACTION, QString::fromLatin1("rename") );
        } else {
            if (( errno == EACCES ) || (errno == EPERM)) {
                error( KIO::ERR_ACCESS_DENIED, dest );
            } else if (errno == EROFS) { // The file is on a read-only filesystem
                error( KIO::ERR_CANNOT_DELETE, src );
            } else {
                error( KIO::ERR_CANNOT_RENAME, src );
            }
        }
        return false;
    }
    return true;
}

#if 0
bool TrashImpl::mkdir( int trashId, const QString& fileId, int permissions )
{
    const QString path = filesPath( trashId, fileId );
    if ( ::mkdir( QFile::encodeName( path ), permissions ) != 0 ) {
        if ( errno == EACCES ) {
            error( KIO::ERR_ACCESS_DENIED, path );
            return false;
        } else if ( errno == ENOSPC ) {
            error( KIO::ERR_DISK_FULL, path );
            return false;
        } else {
            error( KIO::ERR_COULD_NOT_MKDIR, path );
            return false;
        }
    } else {
        if ( permissions != -1 )
            ::chmod( QFile::encodeName( path ), permissions );
    }
    return true;
}
#endif

bool TrashImpl::del( int trashId, const QString& fileId )
{
    QString info = infoPath(trashId, fileId);
    QString file = filesPath(trashId, fileId);

    QCString info_c = QFile::encodeName(info);

    KDE_struct_stat buff;
    if ( KDE_stat( info_c.data(), &buff ) == -1 ) {
        if ( errno == EACCES )
            error( KIO::ERR_ACCESS_DENIED, file );
        else
            error( KIO::ERR_DOES_NOT_EXIST, file );
        return false;
    }

    ::unlink(info_c);

    if ( !synchronousDel( file ) )
        return false;
    fileRemoved();
    return true;
}

bool TrashImpl::synchronousDel( const QString& file )
{
    KURL url;
    url.setPath( file );
    KIO::DeleteJob *job = KIO::del( url, false, false);
    connect( job, SIGNAL( result(KIO::Job *) ),
             this, SLOT( jobFinished(KIO::Job *) ) );
    qApp->eventLoop()->enterLoop();
    return m_lastErrorCode == 0;
}

bool TrashImpl::emptyTrash()
{
    kdDebug() << k_funcinfo << endl;
    // For each known trash directory...
    // ######## TODO: read fstab to know about all partitions, and check for trash dirs on those
    TrashDirMap::const_iterator it = m_trashDirectories.begin();
    for ( ; it != m_trashDirectories.end() ; ++it ) {
        QDir dir;
        const QString infoPath = it.data() + "/info";
        synchronousDel( infoPath );
        dir.mkdir( infoPath );
        const QString filesPath = it.data() + "/files";
        synchronousDel( filesPath );
        dir.mkdir( filesPath );
    }
    fileRemoved();
    return false;
}

TrashImpl::TrashedFileInfoList TrashImpl::list()
{
    TrashedFileInfoList lst;
    // For each known trash directory...
    // ######## TODO: read fstab to know about all partitions, and check for trash dirs on those
    TrashDirMap::const_iterator it = m_trashDirectories.begin();
    for ( ; it != m_trashDirectories.end() ; ++it ) {
        const int trashId = it.key();
        QString infoPath = it.data();
        infoPath += "/info";
        // Code taken from kio_file
        QStrList entryNames = listDir( infoPath );
        //char path_buffer[PATH_MAX];
        //getcwd(path_buffer, PATH_MAX - 1);
        //if ( chdir( infoPathEnc ) )
        //    continue;
        QStrListIterator entryIt( entryNames );
        for (; entryIt.current(); ++entryIt) {
            TrashedFileInfo info;
            if ( infoForFile( trashId, QFile::decodeName( *entryIt ), info ) )
                lst << info;
        }
    }
    return lst;
}

// Returns the entries in a given directory - including "." and ".."
QStrList TrashImpl::listDir( const QString& physicalPath )
{
    const QCString physicalPathEnc = QFile::encodeName( physicalPath );
    kdDebug() << k_funcinfo << "listing " << physicalPath << endl;
    QStrList entryNames;
    DIR *dp = opendir( physicalPathEnc );
    if ( dp == 0 )
        return entryNames;
    KDE_struct_dirent *ep;
    while ( ( ep = KDE_readdir( dp ) ) != 0L )
        entryNames.append( ep->d_name );
    closedir( dp );
    return entryNames;
}

bool TrashImpl::infoForFile( int trashId, const QString& fileId, TrashedFileInfo& info )
{
    kdDebug() << k_funcinfo << trashId << " " << fileId << endl;
    info.trashId = trashId; // easy :)
    info.fileId = fileId; // equally easy
    info.physicalPath = filesPath( trashId, fileId );
    return readInfoFile( infoPath( trashId, fileId ), info );
}

bool TrashImpl::readInfoFile( const QString& infoPath, TrashedFileInfo& info )
{
    KSimpleConfig cfg( infoPath, true );
    if ( !cfg.hasGroup( "Trash Info" ) ) {
        error( KIO::ERR_CANNOT_OPEN_FOR_READING, infoPath );
        return false;
    }
    cfg.setGroup( "Trash Info" );
    info.origPath = cfg.readEntry( "Path" );
    if ( info.origPath.isEmpty() )
        return false; // path is mandatory...
    QString line = cfg.readEntry( "DeletionDate" );
    if ( !line.isEmpty() ) {
        info.deletionDate = QDateTime::fromString( line, Qt::ISODate );
    }
    return true;
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

void TrashImpl::error( int e, const QString& s )
{
    if ( e )
        kdDebug() << k_funcinfo << e << " " << s << endl;
    m_lastErrorCode = e;
    m_lastErrorMessage = s;
}

bool TrashImpl::isEmpty() const
{
    // For each known trash directory...
    // ######## TODO: read fstab to know about all partitions, and check for trash dirs on those
    TrashDirMap::const_iterator it = m_trashDirectories.begin();
    for ( ; it != m_trashDirectories.end() ; ++it ) {
        QString infoPath = it.data();
        infoPath += "/info";

        DIR *dp = opendir( QFile::encodeName( infoPath ) );
        if ( dp )
        {
            struct dirent *ep;
            ep = readdir( dp );
            ep = readdir( dp ); // ignore '.' and '..' dirent
            ep = readdir( dp ); // look for third file
            closedir( dp );
            if ( ep != 0 ) {
                return false; // not empty
            }
        }
    }
    return true;
}

void TrashImpl::fileAdded()
{
    m_config.setGroup( "Status" );
    if ( m_config.readBoolEntry( "Empty", true ) == true ) {
        m_config.writeEntry( "Empty", false );
        m_config.sync();
        refreshTrashIcon();
    }
}

void TrashImpl::fileRemoved()
{
    if ( isEmpty() ) {
        m_config.setGroup( "Status" );
        m_config.writeEntry( "Empty", true );
        m_config.sync();
        refreshTrashIcon();
    }
}

void TrashImpl::refreshTrashIcon()
{
    DCOPRef kdesktop( "kdesktop", "default" );
    kdesktop.call( "refreshTrashIcon" );
}

#include "trashimpl.moc"
