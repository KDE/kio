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

#include <kapplication.h>
#include <kdebug.h>
#include <klocale.h>
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

static const KCmdLineOptions options[] =
{
    { "+protocol", I18N_NOOP( "Protocol name" ), 0 },
    { "+pool", I18N_NOOP( "Protocol name" ), 0 },
    { "+app", I18N_NOOP( "Protocol name" ), 0 },
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

void TrashProtocol::rename(KURL const &oldURL, KURL const &newURL, bool overwrite)
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
            if ( !impl.add( oldURL.path() ) )
                error( impl.lastErrorCode(), impl.lastErrorMessage() );
            else
                finished();
            return;
        } else {
            // TODO
//            QString trashDir = impl.resolveTrash...
//            if ( !impl.rename( oldURL.path(), trashDir + ...
            return;
        }
    }
    error( KIO::ERR_UNSUPPORTED_ACTION, "rename" );
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
        // TODO Read metadata file (stat deleted file), and return information
        error( KIO::ERR_UNSUPPORTED_ACTION, "stat" );
    }
}

void TrashProtocol::listDir(const KURL& url)
{
    INIT_IMPL;
    kdDebug() << "listdir: " << url << endl;
    if (url.path().length() <= 1)
    {
        listRoot();
        return;
    }
    // TODO [see stat]
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

void TrashProtocol::listRoot()
{
    INIT_IMPL;
#if 0
    KIO::UDSEntry   entry;

    {
        QString url="devices:/"+(*it); ++it;
        QString name=*it; ++it;
        ++it; ++it;
        QString type=*it; ++it; ++it;
        createFileEntry(entry,name,url,type);
        listEntry(entry,false);
        count++;
    }
    totalSize(count);
    listEntry(entry, true);

    finished();
#endif
}

void TrashProtocol::get( const KURL& url )
{
    INIT_IMPL;
/*
  mimeType(...);
  data(output);
  finished();
*/
}

static void createFileEntry(KIO::UDSEntry& entry, const QString& name, const QString& url, const QString& mime)
{
    entry.clear();
    addAtom(entry, KIO::UDS_NAME, 0, name);
    addAtom(entry, KIO::UDS_FILE_TYPE, S_IFDIR);//REG);
    addAtom(entry, KIO::UDS_URL, 0, url);
    addAtom(entry, KIO::UDS_ACCESS, 0500); // readonly
    addAtom(entry, KIO::UDS_MIME_TYPE, 0, mime);
    addAtom(entry, KIO::UDS_SIZE, 0);
    addAtom(entry, KIO::UDS_GUESSED_MIME_TYPE, 0, "inode/directory");
    addAtom(entry, KIO::UDS_CREATION_TIME,1);
    addAtom(entry, KIO::UDS_MODIFICATION_TIME,time(0));
}
