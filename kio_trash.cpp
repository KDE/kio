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

#include "kio_trash.h"
#include <kio/job.h>

#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
#include <klargefile.h>
#include <kprocess.h>

#include <dcopclient.h>
#include <qdatastream.h>
#include <qtextstream.h>

#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <kcmdlineargs.h>
#include <qfile.h>

static const KCmdLineOptions options[] =
{
    { "+protocol", I18N_NOOP( "Protocol name" ), 0 },
    { "+pool", I18N_NOOP( "Socket name" ), 0 },
    { "+app", I18N_NOOP( "Socket name" ), 0 },
    KCmdLineLastOption
};

extern "C" {
    int kdemain( int argc, char **argv )
    {
        //KInstance instance( "kio_trash" );
        // KApplication is necessary to use kio_file
        KCmdLineArgs::init(argc, argv, "kio_trash", 0, 0, 0, 0);
        KCmdLineArgs::addCmdLineOptions( options );
        KApplication app( false, true );

        TrashProtocol slave(argv[1],argv[2], argv[3]);
        slave.dispatchLoop();
        return 0;
    }
}

#define INIT_IMPL \
    if ( !impl.init() ) { \
        error( impl.lastErrorCode(), impl.lastErrorMessage() ); \
        return; \
    }
typedef TrashImpl::TrashedFileInfo TrashedFileInfo;
typedef TrashImpl::TrashedFileInfoList TrashedFileInfoList;

TrashProtocol::TrashProtocol( const QCString& protocol, const QCString &pool, const QCString &app)
    : SlaveBase(protocol,  pool, app )
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

// Helper method. Creates a URL with the format trash:/trashid-fileid or
// trash:/trashid-fileid/relativePath/To/File for a file inside a trashed directory.
QString TrashProtocol::makeURL( int trashId, const QString& fileId, const QString& relativePath )
{
    QString url = "trash:/";
    url += QString::number( trashId );
    url += '-';
    url += fileId;
    if ( !relativePath.isEmpty() ) {
        url += '/';
        url += relativePath;
    }
    return url;
}

// Helper method. Parses a trash URL with the URL scheme defined in makeURL.
// The trash:/ URL itself isn't parsed here, must be caught by the caller before hand.
bool TrashProtocol::parseURL( const KURL& url, int& trashId, QString& fileId, QString& relativePath )
{
    if ( url.protocol() != "trash" )
        return false;
    const QString path = url.path();
    int start = 0;
    if ( path[0] == '/' ) // always true I hope
        start = 1;
    int slashPos = path.find( '-', 0 ); // don't match leading slash
    if ( slashPos <= 0 )
        return false;
    bool ok = false;
    trashId = path.mid( start, slashPos - start ).toInt( &ok );
    Q_ASSERT( ok );
    if ( !ok )
        return false;
    start = slashPos + 1;
    slashPos = path.find( '/', start );
    if ( slashPos <= 0 ) {
        fileId = path.mid( start );
        relativePath = QString::null;
        return true;
    }
    fileId = path.mid( start, slashPos - start );
    relativePath = path.mid( slashPos + 1 );
    return true;
}

void TrashProtocol::rename(const KURL &oldURL, const KURL &newURL, bool overwrite)
{
    INIT_IMPL;

    kdDebug()<<"TrashProtocol::rename(): old="<<oldURL<<" new="<<newURL<<" overwrite=" << overwrite<<endl;

    if ( oldURL.protocol() == "trash" && newURL.protocol() == "trash" ) {
        error( KIO::ERR_CANNOT_RENAME, oldURL.prettyURL() );
        return;
    }
    if ( oldURL.protocol() == "trash" && newURL.isLocalFile() ) {
        // Extracting (e.g. via dnd). Ignore original location.
        // TODO
        //return;
    } else if ( oldURL.isLocalFile() && newURL.protocol() == "trash" ) {
        QString dir = newURL.directory();
        // Trashing a file
        if ( dir.length() <= 1 ) // new toplevel entry
        {
            // ## we should use parseURL to give the right filename to createInfo
            int trashId;
            QString fileId;
            if ( !impl.createInfo( oldURL.path(), trashId, fileId ) ) {
                error( impl.lastErrorCode(), impl.lastErrorMessage() );
            } else {
                if ( !impl.tryRename( oldURL.path(), trashId, fileId ) ) {
                    (void)impl.deleteInfo( trashId, fileId );
                    error( impl.lastErrorCode(), impl.lastErrorMessage() );
                } else
                    finished();
            }
            return;
        } else {
            // Well it's not allowed to add a file to an existing deleted directory.
            // During the deletion itself, we either rename() in one go or copy+del, so we never rename()...
            error( KIO::ERR_ACCESS_DENIED, newURL.prettyURL() );
            return;
        }
    }
    error( KIO::ERR_UNSUPPORTED_ACTION, "rename" );
}

void TrashProtocol::copy( const KURL &src, const KURL &dest, int permissions, bool overwrite )
{
    INIT_IMPL;

    kdDebug()<<"TrashProtocol::copy(): " << src << " " << dest << endl;

    if ( src.protocol() == "trash" && dest.protocol() == "trash" ) {
        error( KIO::ERR_UNSUPPORTED_ACTION, i18n( "This file is already in the trash bin." ) );
        return;
    }

    if ( src.protocol() == "trash" && dest.isLocalFile() ) {
        // Extracting (e.g. via dnd). Ignore original location.
        // TODO
        //return;
    } else if ( src.isLocalFile() && dest.protocol() == "trash" ) {
        QString dir = dest.directory();
        // Trashing a copy file
        if ( dir.length() <= 1 ) // new toplevel entry
        {
            int trashId;
            QString fileId;
            if ( !impl.createInfo( src.path(), trashId, fileId ) ) {
                error( impl.lastErrorCode(), impl.lastErrorMessage() );
            } else {
                // kio_file's copy() method is quite complex (in order to be fast), let's just call it...
                KURL filesPath;
                filesPath.setPath( impl.filesPath( trashId, fileId ) );
                kdDebug() << k_funcinfo << "copying " << src << " to " << filesPath << endl;
                KIO::Job* job = KIO::file_copy( src, filesPath, permissions, overwrite, false, false );
                connect( job, SIGNAL( result( KIO::Job* ) ), SLOT( slotCopyResult( KIO::Job* ) ) );
                m_curTrashId = trashId;
                m_curFileId = fileId;
                qApp->enter_loop();
            }
        } else {
            // It's not allowed to add a file to an existing deleted directory.
            // But during the deletion itself, we'll be called for subfiles
            // TODO
            error( KIO::ERR_ACCESS_DENIED, dest.prettyURL() );
            return;
        }
        return;
    }
    error( KIO::ERR_UNSUPPORTED_ACTION, "copy" );
}

void TrashProtocol::slotCopyResult( KIO::Job* job )
{
    kdDebug() << k_funcinfo << endl;
    if ( job->error() ) {
        (void)impl.deleteInfo( m_curTrashId, m_curFileId );
        error( job->error(), job->errorText() );
    } else {
        finished();
    }
    qApp->exit_loop();
}

static void addAtom(KIO::UDSEntry& entry, unsigned int ID, long l, const QString& s = QString::null)
{
    KIO::UDSAtom atom;
    atom.m_uds = ID;
    atom.m_long = l;
    atom.m_str = s;
    entry.append(atom);
}

void TrashProtocol::createTopLevelDirEntry(KIO::UDSEntry& entry, const QString& name, const QString& url)
{
    entry.clear();
    addAtom(entry, KIO::UDS_NAME, 0, name);
    addAtom(entry, KIO::UDS_FILE_TYPE, S_IFDIR);
    addAtom(entry, KIO::UDS_ACCESS, 0700);
    addAtom(entry, KIO::UDS_MIME_TYPE, 0, "inode/directory");
    if ( !url.isEmpty() )
        addAtom(entry, KIO::UDS_URL, 0, url);
    addAtom(entry, KIO::UDS_USER, 0, m_userName);
    addAtom(entry, KIO::UDS_GROUP, 0, m_groupName);
}

void TrashProtocol::stat(const KURL& url)
{
    INIT_IMPL;
    const QString path = url.path();
    if( path.isEmpty() || path == "/" ) {
        // The root is "virtual" - it's not a single physical directory
        KIO::UDSEntry entry;
        createTopLevelDirEntry( entry, "/", QString::null );
        statEntry( entry );
        finished();
    } else {
        int trashId;
        QString fileId, relativePath;

        bool ok = parseURL(url, trashId, fileId, relativePath);

        if ( !ok ) {
            error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.prettyURL() ) );
            return;
        }

        TrashedFileInfo info;
        ok = impl.infoForFile( trashId, fileId, info );
        if ( !ok ) {
            error( impl.lastErrorCode(), impl.lastErrorMessage() );
            return;
        }

        QString filePath = info.physicalPath;
        if ( !relativePath.isEmpty() ) {
            filePath += "/";
            filePath += relativePath;
        }

        QString fileName = filePath.section('/', -1, -1, QString::SectionSkipEmpty);

        QString fileURL = QString::null;
        if ( url.path().length() > 1 ) {
            fileURL = url.url();
        }

        KIO::UDSEntry entry;
        ok = createUDSEntry(filePath, fileName, fileURL, entry);

        if ( !ok ) {
            error( KIO::ERR_COULD_NOT_STAT, url.prettyURL() );
        }

        statEntry( entry );
	finished();
    }
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
    bool ok = parseURL( url, trashId, fileId, relativePath );
    if ( !ok ) {
        error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.prettyURL() ) );
        return;
    }
    // Get info for fileId
    TrashedFileInfo info;
    ok = impl.infoForFile( trashId, fileId, info );
    if ( !ok ) {
        error( impl.lastErrorCode(), impl.lastErrorMessage() );
        return;
    }
    QString physicalPath = info.physicalPath;
    if ( !relativePath.isEmpty() ) {
        physicalPath += "/";
        physicalPath += relativePath;
    }
    // List subdir. Can't use kio_file here since we provide our own info...
    QStrList entryNames = impl.listDir( physicalPath );
    totalSize( entryNames.count() );
    KIO::UDSEntry entry;
    QStrListIterator entryIt( entryNames );
    for (; entryIt.current(); ++entryIt) {
        const QString fileName = QFile::decodeName( entryIt.current() );
        const QString filePath = physicalPath + "/" + fileName;
        // shouldn't be necessary
        //const QString url = makeURL( trashId, fileId, relativePath + "/" + fileName );
        entry.clear();
        if ( createUDSEntry( filePath, fileName, QString::null /*url*/, entry ) )
            listEntry( entry, false );
    }
    entry.clear();
    listEntry( entry, true );
    finished();
}

bool TrashProtocol::createUDSEntry( const QString& physicalPath, const QString& fileName, const QString& url, KIO::UDSEntry& entry )
{
    KDE_struct_stat buff;
    if ( KDE_stat( QFile::encodeName( physicalPath ), &buff ) == -1 ) {
        kdWarning() << "couldn't stat " << physicalPath << endl;
        return false;
    }
    // TODO symlinks

    mode_t type = buff.st_mode & S_IFMT; // extract file type
    mode_t access = buff.st_mode & 07777; // extract permissions
    access &= 07555; // make it readonly, since it's in the trashcan
    addAtom( entry, KIO::UDS_NAME, 0, fileName );
    addAtom( entry, KIO::UDS_FILE_TYPE, type );
    if ( !url.isEmpty() )
        addAtom( entry, KIO::UDS_URL, 0, url );
    addAtom( entry, KIO::UDS_ACCESS, access );
    addAtom( entry, KIO::UDS_SIZE, buff.st_size );
    addAtom( entry, KIO::UDS_USER, 0, m_userName ); // assumption
    addAtom( entry, KIO::UDS_GROUP, 0, m_groupName ); // assumption
    addAtom( entry, KIO::UDS_MODIFICATION_TIME, buff.st_mtime );
    addAtom( entry, KIO::UDS_ACCESS_TIME, buff.st_atime ); // ## or use it for deletion time?
    return true;
}

void TrashProtocol::listRoot()
{
    INIT_IMPL;
    const TrashedFileInfoList lst = impl.list();
    totalSize( lst.count() );
    KIO::UDSEntry entry;
    for ( TrashedFileInfoList::ConstIterator it = lst.begin(); it != lst.end(); ++it ) {
        const QString url = makeURL( (*it).trashId, (*it).fileId, QString::null );
        KURL origURL;
        origURL.setPath( (*it).origPath );
        entry.clear();
        if ( createUDSEntry( (*it).physicalPath, origURL.fileName(), url, entry ) )
            listEntry( entry, false );
    }
    entry.clear();
    listEntry( entry, true );
    finished();
}

void TrashProtocol::put( const KURL& url, int /*permissions*/, bool /*overwrite*/, bool /*resume*/ )
{
    INIT_IMPL;
    kdDebug() << "put: " << url << endl;
    // create deleted file. We need to get the mtime and original location from metadata...
    // Maybe we can find the info file for url.fileName(), in case ::rename() was called first, and failed...
    error( KIO::ERR_ACCESS_DENIED, url.prettyURL() );
}

void TrashProtocol::mkdir( const KURL& url, int )
{
    INIT_IMPL;
    // create info about deleted dir
    kdDebug() << "mkdir: " << url << endl;
    error( KIO::ERR_ACCESS_DENIED, url.prettyURL() );
}

void TrashProtocol::get( const KURL& )
{
    INIT_IMPL;
/*
  mimeType(...);
  data(output);
  finished();
*/
}

#include "kio_trash.moc"
