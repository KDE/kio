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
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kio_trash.h"
#include <kio/job.h>

#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
#include <kde_file.h>
#include <kcmdlineargs.h>
#include <kmimetype.h>
#include <kprocess.h>

#include <dcopclient.h>
#include <qdatastream.h>
#include <qtextstream.h>
#include <qfile.h>
#include <qeventloop.h>
//Added by qt3to4:
#include <Q3StrList>

#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

static const KCmdLineOptions options[] =
{
    { "+protocol", I18N_NOOP( "Protocol name" ), 0 },
    { "+pool", I18N_NOOP( "Socket name" ), 0 },
    { "+app", I18N_NOOP( "Socket name" ), 0 },
    KCmdLineLastOption
};

extern "C" {
    int KDE_EXPORT kdemain( int argc, char **argv )
    {
        //KInstance instance( "kio_trash" );
        // KApplication is necessary to use kio_file
        putenv(strdup("SESSION_MANAGER="));
        KApplication::disableAutoDcopRegistration();
        KCmdLineArgs::init(argc, argv, "kio_trash", 0, 0, 0);
        KCmdLineArgs::addCmdLineOptions( options );
        KApplication app( false );

        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
        TrashProtocol slave( args->arg(0), args->arg(1), args->arg(2) );
        slave.dispatchLoop();
        return 0;
    }
}

#define INIT_IMPL \
    if ( !impl.init() ) { \
        error( impl.lastErrorCode(), impl.lastErrorMessage() ); \
        return; \
    }

TrashProtocol::TrashProtocol( const QByteArray& protocol, const QByteArray &pool, const QByteArray &app)
    : SlaveBase(protocol, pool, app )
{
    struct passwd *user = getpwuid( getuid() );
    if ( user )
        m_userName = QString::fromLatin1(user->pw_name);
    struct group *grp = getgrgid( getgid() );
    if ( grp )
        m_groupName = QString::fromLatin1(grp->gr_name);
}

TrashProtocol::~TrashProtocol()
{
}

void TrashProtocol::enterLoop()
{
    QEventLoop eventLoop;
    connect(this, SIGNAL(leaveModality()),
        &eventLoop, SLOT(quit()));
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

void TrashProtocol::restore( const KURL& trashURL )
{
    int trashId;
    QString fileId, relativePath;
    bool ok = TrashImpl::parseURL( trashURL, trashId, fileId, relativePath );
    if ( !ok ) {
        error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( trashURL.prettyURL() ) );
        return;
    }
    TrashedFileInfo info;
    ok = impl.infoForFile( trashId, fileId, info );
    if ( !ok ) {
        error( impl.lastErrorCode(), impl.lastErrorMessage() );
        return;
    }
    KURL dest;
    dest.setPath( info.origPath );
    if ( !relativePath.isEmpty() )
        dest.addPath( relativePath );

    // Check that the destination directory exists, to improve the error code in case it doesn't.
    const QString destDir = dest.directory();
    KDE_struct_stat buff;
    if ( KDE_lstat( QFile::encodeName( destDir ), &buff ) == -1 ) {
        error( KIO::ERR_SLAVE_DEFINED,
               i18n( "The directory %1 does not exist anymore, so it is not possible to restore this item to its original location. "
                     "You can either recreate that directory and use the restore operation again, or drag the item anywhere else to restore it." ).arg( destDir ) );
        return;
    }

    copyOrMove( trashURL, dest, false /*overwrite*/, Move );
}

void TrashProtocol::rename( const KURL &oldURL, const KURL &newURL, bool overwrite )
{
    INIT_IMPL;

    kdDebug()<<"TrashProtocol::rename(): old="<<oldURL<<" new="<<newURL<<" overwrite=" << overwrite<<endl;

    if ( oldURL.protocol() == "trash" && newURL.protocol() == "trash" ) {
        error( KIO::ERR_CANNOT_RENAME, oldURL.prettyURL() );
        return;
    }

    copyOrMove( oldURL, newURL, overwrite, Move );
}

void TrashProtocol::copy( const KURL &src, const KURL &dest, int /*permissions*/, bool overwrite )
{
    INIT_IMPL;

    kdDebug()<<"TrashProtocol::copy(): " << src << " " << dest << endl;

    if ( src.protocol() == "trash" && dest.protocol() == "trash" ) {
        error( KIO::ERR_UNSUPPORTED_ACTION, i18n( "This file is already in the trash bin." ) );
        return;
    }

    copyOrMove( src, dest, overwrite, Copy );
}

void TrashProtocol::copyOrMove( const KURL &src, const KURL &dest, bool overwrite, CopyOrMove action )
{
    if ( src.protocol() == "trash" && dest.isLocalFile() ) {
        // Extracting (e.g. via dnd). Ignore original location stored in info file.
        int trashId;
        QString fileId, relativePath;
        bool ok = TrashImpl::parseURL( src, trashId, fileId, relativePath );
        if ( !ok ) {
            error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( src.prettyURL() ) );
            return;
        }
        const QString destPath = dest.path();
        if ( QFile::exists( destPath ) ) {
            if ( overwrite ) {
                ok = QFile::remove( destPath );
                Q_ASSERT( ok ); // ### TODO
            } else {
                error( KIO::ERR_FILE_ALREADY_EXIST, destPath );
                return;
            }
        }

        if ( action == Move ) {
            kdDebug() << "calling moveFromTrash(" << destPath << " " << trashId << " " << fileId << ")" << endl;
            ok = impl.moveFromTrash( destPath, trashId, fileId, relativePath );
        } else { // Copy
            kdDebug() << "calling copyFromTrash(" << destPath << " " << trashId << " " << fileId << ")" << endl;
            ok = impl.copyFromTrash( destPath, trashId, fileId, relativePath );
        }
        if ( !ok ) {
            error( impl.lastErrorCode(), impl.lastErrorMessage() );
        } else {
            if ( action == Move && relativePath.isEmpty() )
                (void)impl.deleteInfo( trashId, fileId );
            finished();
        }
        return;
    } else if ( src.isLocalFile() && dest.protocol() == "trash" ) {
        QString dir = dest.directory();
        //kdDebug() << "trashing a file to " << dir << endl;
        // Trashing a file
        // We detect the case where this isn't normal trashing, but
        // e.g. if kwrite tries to save (moving tempfile over destination)
        if ( dir.length() <= 1 && src.fileName() == dest.fileName() ) // new toplevel entry
        {
            const QString srcPath = src.path();
            // In theory we should use TrashImpl::parseURL to give the right filename to createInfo,
            // in case the trash URL didn't contain the same filename as srcPath.
            // But this can only happen with copyAs/moveAs, not available in the GUI
            // for the trash (New/... or Rename from iconview/listview).
            int trashId;
            QString fileId;
            if ( !impl.createInfo( srcPath, trashId, fileId ) ) {
                error( impl.lastErrorCode(), impl.lastErrorMessage() );
            } else {
                bool ok;
                if ( action == Move ) {
                    kdDebug() << "calling moveToTrash(" << srcPath << " " << trashId << " " << fileId << ")" << endl;
                    ok = impl.moveToTrash( srcPath, trashId, fileId );
                } else { // Copy
                    kdDebug() << "calling copyToTrash(" << srcPath << " " << trashId << " " << fileId << ")" << endl;
                    ok = impl.copyToTrash( srcPath, trashId, fileId );
                }
                if ( !ok ) {
                    (void)impl.deleteInfo( trashId, fileId );
                    error( impl.lastErrorCode(), impl.lastErrorMessage() );
                } else {
                    // Inform caller of the final URL. Used by konq_undo.
                    const KURL url = impl.makeURL( trashId, fileId, QString() );
                    setMetaData( "trashURL-" + srcPath, url.url() );
                    finished();
                }
            }
            return;
        } else {
            kdDebug() << "returning KIO::ERR_ACCESS_DENIED, it's not allowed to add a file to an existing trash directory" << endl;
            // It's not allowed to add a file to an existing trash directory.
            error( KIO::ERR_ACCESS_DENIED, dest.prettyURL() );
            return;
        }
    } else
        error( KIO::ERR_UNSUPPORTED_ACTION, "should never happen" );
}

void TrashProtocol::createTopLevelDirEntry(KIO::UDSEntry& entry)
{
    entry.clear();
    entry.insert( KIO::UDS_NAME, QString::fromLatin1("."));
    entry.insert( KIO::UDS_FILE_TYPE, S_IFDIR);
    entry.insert( KIO::UDS_ACCESS, 0700);
    entry.insert( KIO::UDS_MIME_TYPE, QString::fromLatin1("inode/directory"));
    entry.insert( KIO::UDS_USER, m_userName);
    entry.insert( KIO::UDS_GROUP, m_groupName);
}

void TrashProtocol::stat(const KURL& url)
{
    INIT_IMPL;
    const QString path = url.path();
    if( path.isEmpty() || path == "/" ) {
        // The root is "virtual" - it's not a single physical directory
        KIO::UDSEntry entry;
        createTopLevelDirEntry( entry );
        statEntry( entry );
        finished();
    } else {
        int trashId;
        QString fileId, relativePath;

        bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );

        if ( !ok ) {
            // ######## do we still need this?
            kdDebug() << k_funcinfo << url << " looks fishy, returning does-not-exist" << endl;
            // A URL like trash:/file simply means that CopyJob is trying to see if
            // the destination exists already (it made up the URL by itself).
            error( KIO::ERR_DOES_NOT_EXIST, url.prettyURL() );
            //error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.prettyURL() ) );
            return;
        }

        const QString filePath = impl.physicalPath( trashId, fileId, relativePath );
        if ( filePath.isEmpty() ) {
            error( impl.lastErrorCode(), impl.lastErrorMessage() );
            return;
        }

        QString fileName = filePath.section('/', -1, -1, QString::SectionSkipEmpty);

        QString fileURL = QString();
        if ( url.path().length() > 1 ) {
            fileURL = url.url();
        }

        KIO::UDSEntry entry;
        TrashedFileInfo info;
        ok = impl.infoForFile( trashId, fileId, info );
        if ( ok )
            ok = createUDSEntry( filePath, fileName, fileURL, entry, info );

        if ( !ok ) {
            error( KIO::ERR_COULD_NOT_STAT, url.prettyURL() );
        }

        statEntry( entry );
        finished();
    }
}

void TrashProtocol::del( const KURL &url, bool /*isfile*/ )
{
    int trashId;
    QString fileId, relativePath;

    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    if ( !ok ) {
        error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.prettyURL() ) );
        return;
    }

    ok = relativePath.isEmpty();
    if ( !ok ) {
        error( KIO::ERR_ACCESS_DENIED, url.prettyURL() );
        return;
    }

    ok = impl.del(trashId, fileId);
    if ( !ok ) {
        error( impl.lastErrorCode(), impl.lastErrorMessage() );
        return;
    }

    finished();
}

void TrashProtocol::listDir(const KURL& url)
{
    INIT_IMPL;
    kdDebug() << "listdir: " << url << endl;
    if ( url.path().length() <= 1 ) {
        listRoot();
        return;
    }
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    if ( !ok ) {
        error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.prettyURL() ) );
        return;
    }
    //was: const QString physicalPath = impl.physicalPath( trashId, fileId, relativePath );

    // Get info for deleted directory - the date of deletion and orig path will be used
    // for all the items in it, and we need the physicalPath.
    TrashedFileInfo info;
    ok = impl.infoForFile( trashId, fileId, info );
    if ( !ok || info.physicalPath.isEmpty() ) {
        error( impl.lastErrorCode(), impl.lastErrorMessage() );
        return;
    }
    if ( !relativePath.isEmpty() ) {
        info.physicalPath += "/";
        info.physicalPath += relativePath;
    }

    // List subdir. Can't use kio_file here since we provide our own info...
    kdDebug() << k_funcinfo << "listing " << info.physicalPath << endl;
    Q3StrList entryNames = impl.listDir( info.physicalPath );
    totalSize( entryNames.count() );
    KIO::UDSEntry entry;
    Q3StrListIterator entryIt( entryNames );
    for (; entryIt.current(); ++entryIt) {
        QString fileName = QFile::decodeName( entryIt.current() );
        if ( fileName == ".." )
            continue;
        const QString filePath = info.physicalPath + "/" + fileName;
        // shouldn't be necessary
        //const QString url = TrashImpl::makeURL( trashId, fileId, relativePath + "/" + fileName );
        entry.clear();
        TrashedFileInfo infoForItem( info );
        infoForItem.origPath += '/';
        infoForItem.origPath += fileName;
        if ( ok && createUDSEntry( filePath, fileName, QString() /*url*/, entry, infoForItem ) ) {
            listEntry( entry, false );
        }
    }
    entry.clear();
    listEntry( entry, true );
    finished();
}

bool TrashProtocol::createUDSEntry( const QString& physicalPath, const QString& fileName, const QString& url, KIO::UDSEntry& entry, const TrashedFileInfo& info )
{
    QByteArray physicalPath_c = QFile::encodeName( physicalPath );
    KDE_struct_stat buff;
    if ( KDE_lstat( physicalPath_c, &buff ) == -1 ) {
        kdWarning() << "couldn't stat " << physicalPath << endl;
        return false;
    }
    if (S_ISLNK(buff.st_mode)) {
        char buffer2[ 1000 ];
        int n = readlink( physicalPath_c, buffer2, 1000 );
        if ( n != -1 ) {
            buffer2[ n ] = 0;
        }

        entry.insert( KIO::UDS_LINK_DEST, QFile::decodeName( buffer2 ) );
        // Follow symlink
        if ( KDE_stat( physicalPath_c, &buff ) == -1 ) {
            // It is a link pointing to nowhere
            buff.st_mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
            buff.st_mtime = 0;
            buff.st_atime = 0;
            buff.st_size = 0;
        }
    }
    mode_t type = buff.st_mode & S_IFMT; // extract file type
    mode_t access = buff.st_mode & 07777; // extract permissions
    access &= 07555; // make it readonly, since it's in the trashcan
    entry.insert( KIO::UDS_NAME, fileName );
    entry.insert( KIO::UDS_FILE_TYPE, type );
    if ( !url.isEmpty() )
        entry.insert( KIO::UDS_URL, url );

    KMimeType::Ptr mt = KMimeType::findByPath( physicalPath, buff.st_mode );
    entry.insert( KIO::UDS_MIME_TYPE, mt->name() );
    entry.insert( KIO::UDS_ACCESS, access );
    entry.insert( KIO::UDS_SIZE, buff.st_size );
    entry.insert( KIO::UDS_USER, m_userName ); // assumption
    entry.insert( KIO::UDS_GROUP, m_groupName ); // assumption
    entry.insert( KIO::UDS_MODIFICATION_TIME, buff.st_mtime );
    entry.insert( KIO::UDS_ACCESS_TIME, buff.st_atime ); // ## or use it for deletion time?
    entry.insert( KIO::UDS_EXTRA, info.origPath );
    entry.insert( KIO::UDS_EXTRA, info.deletionDate.toString( Qt::ISODate ) );
    return true;
}

void TrashProtocol::listRoot()
{
    INIT_IMPL;
    const TrashedFileInfoList lst = impl.list();
    totalSize( lst.count() );
    KIO::UDSEntry entry;
    createTopLevelDirEntry( entry );
    listEntry( entry, false );
    for ( TrashedFileInfoList::ConstIterator it = lst.begin(); it != lst.end(); ++it ) {
        const KURL url = TrashImpl::makeURL( (*it).trashId, (*it).fileId, QString() );
        KURL origURL;
        origURL.setPath( (*it).origPath );
        entry.clear();
        if ( createUDSEntry( (*it).physicalPath, origURL.fileName(), url.url(), entry, *it ) )
            listEntry( entry, false );
    }
    entry.clear();
    listEntry( entry, true );
    finished();
}

void TrashProtocol::special( const QByteArray & data )
{
    INIT_IMPL;
    QDataStream stream( data );
    int cmd;
    stream >> cmd;

    switch (cmd) {
    case 1:
        impl.emptyTrash();
        finished();
        break;
    case 2:
        impl.migrateOldTrash();
        finished();
        break;
    case 3:
    {
        KURL url;
        stream >> url;
        restore( url );
        break;
    }
    default:
        kdWarning(7116) << "Unknown command in special(): " << cmd << endl;
        error( KIO::ERR_UNSUPPORTED_ACTION, QString::number(cmd) );
        break;
    }
}

void TrashProtocol::put( const KURL& url, int /*permissions*/, bool /*overwrite*/, bool /*resume*/ )
{
    INIT_IMPL;
    kdDebug() << "put: " << url << endl;
    // create deleted file. We need to get the mtime and original location from metadata...
    // Maybe we can find the info file for url.fileName(), in case ::rename() was called first, and failed...
    error( KIO::ERR_ACCESS_DENIED, url.prettyURL() );
}

void TrashProtocol::get( const KURL& url )
{
    INIT_IMPL;
    kdDebug() << "get() : " << url << endl;
    if ( !url.isValid() ) {
        kdDebug() << kdBacktrace() << endl;
        error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.url() ) );
        return;
    }
    if ( url.path().length() <= 1 ) {
        error( KIO::ERR_IS_DIRECTORY, url.prettyURL() );
        return;
    }
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    if ( !ok ) {
        error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.prettyURL() ) );
        return;
    }
    const QString physicalPath = impl.physicalPath( trashId, fileId, relativePath );
    if ( physicalPath.isEmpty() ) {
        error( impl.lastErrorCode(), impl.lastErrorMessage() );
        return;
    }

    // Usually we run jobs in TrashImpl (for e.g. future kdedmodule)
    // But for this one we wouldn't use DCOP for every bit of data...
    KURL fileURL;
    fileURL.setPath( physicalPath );
    KIO::Job* job = KIO::get( fileURL );
    connect( job, SIGNAL( data( KIO::Job*, const QByteArray& ) ),
             this, SLOT( slotData( KIO::Job*, const QByteArray& ) ) );
    connect( job, SIGNAL( mimetype( KIO::Job*, const QString& ) ),
             this, SLOT( slotMimetype( KIO::Job*, const QString& ) ) );
    connect( job, SIGNAL( result(KIO::Job *) ),
             this, SLOT( jobFinished(KIO::Job *) ) );
    enterLoop();
}

void TrashProtocol::slotData( KIO::Job*, const QByteArray&arr )
{
    data( arr );
}

void TrashProtocol::slotMimetype( KIO::Job*, const QString& mt )
{
    mimeType( mt );
}

void TrashProtocol::jobFinished( KIO::Job* job )
{
    if ( job->error() )
        error( job->error(), job->errorText() );
    else
        finished();
    emit leaveModality();
}

#if 0
void TrashProtocol::mkdir( const KURL& url, int /*permissions*/ )
{
    INIT_IMPL;
    // create info about deleted dir
    // ############ Problem: we don't know the original path.
    // Let's try to avoid this case (we should get to copy() instead, for local files)
    kdDebug() << "mkdir: " << url << endl;
    QString dir = url.directory();

    if ( dir.length() <= 1 ) // new toplevel entry
    {
        // ## we should use TrashImpl::parseURL to give the right filename to createInfo
        int trashId;
        QString fileId;
        if ( !impl.createInfo( url.path(), trashId, fileId ) ) {
            error( impl.lastErrorCode(), impl.lastErrorMessage() );
        } else {
            if ( !impl.mkdir( trashId, fileId, permissions ) ) {
                (void)impl.deleteInfo( trashId, fileId );
                error( impl.lastErrorCode(), impl.lastErrorMessage() );
            } else
                finished();
        }
    } else {
        // Well it's not allowed to add a directory to an existing deleted directory.
        error( KIO::ERR_ACCESS_DENIED, url.prettyURL() );
    }
}
#endif

#include "kio_trash.moc"
