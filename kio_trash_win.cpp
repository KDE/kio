/* This file is part of the KDE project
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

#define QT_NO_CAST_FROM_ASCII
#define _WIN32_WINNT 0x0500

#include "kio_trash_win.h"
#include <kio/job.h>

#include <kdebug.h>
#include <klocale.h>
#include <kde_file.h>
#include <kcomponentdata.h>
#include <kmimetype.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>

#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <objbase.h>

extern "C" {
    int KDE_EXPORT kdemain( int argc, char **argv )
    {
        // necessary to use other kio slaves
        KComponentData componentData("kio_trash" );
        QCoreApplication app(argc, argv);

        // start the slave
        TrashProtocol slave( argv[1], argv[2], argv[3] );
        slave.dispatchLoop();
        return 0;
    }
}

#define KDE_SECONDS_SINCE_1601  11644473600LL
#define KDE_USEC_IN_SEC             1000000LL
static inline time_t filetimeToTime_t(const FILETIME *time)
{
  ULARGE_INTEGER i64;
  i64.LowPart   = time->dwLowDateTime;
  i64.HighPart  = time->dwHighDateTime;
  i64.QuadPart /= KDE_USEC_IN_SEC * 10;
  i64.QuadPart -= KDE_SECONDS_SINCE_1601;
  return i64.QuadPart;
}

TrashProtocol::TrashProtocol( const QByteArray& protocol, const QByteArray &pool, const QByteArray &app)
    : SlaveBase(protocol, pool, app )
{
    LPITEMIDLIST  iilTrash;
    IShellFolder *isfDesktop;

    OleInitialize (NULL);
    // we assume that this will always work - if not we've a bigger problem than a kio_trash crash...
    SHGetFolderLocation( NULL, CSIDL_BITBUCKET, 0, 0, &iilTrash );
    SHGetDesktopFolder( &isfDesktop );
    isfDesktop->BindToObject( iilTrash, NULL, IID_IShellFolder2, (void**)&m_isfTrashFolder );
    isfDesktop->Release();
    ILFree( iilTrash );
    SHGetMalloc( &m_pMalloc );
}

TrashProtocol::~TrashProtocol()
{
    if( m_pMalloc )
        m_pMalloc->Release();
    if( m_isfTrashFolder )
        m_isfTrashFolder->Release();
}

void TrashProtocol::restore( const KUrl& trashURL, const KUrl &destURL )
{
    LPITEMIDLIST  pidl = NULL;
    LPCONTEXTMENU pCtxMenu = NULL;

    const QString path = trashURL.path().mid(1).replace(QLatin1Char('/'), QLatin1Char('\\'));
    LPWSTR lpFile = (LPWSTR)path.utf16();
    HRESULT res = m_isfTrashFolder->ParseDisplayName( 0, 0, lpFile, 0, &pidl, 0 );
    bool bOk = translateError( res );
    if( !bOk )
        return;

    res = m_isfTrashFolder->GetUIObjectOf( 0, 1, (LPCITEMIDLIST *)&pidl, IID_IContextMenu, NULL, (LPVOID *)&pCtxMenu);
    bOk = translateError( res );
    if( !bOk )
        return;

    // this looks hacky but it's the only solution I found so far...
    HMENU hmenuCtx = CreatePopupMenu();
    res = pCtxMenu->QueryContextMenu(hmenuCtx, 0, 1, 0x00007FFF, CMF_NORMAL);
    bOk = translateError( res );
    if( !bOk )
        return;

    UINT uiCommand = ~0U;
    char verb[MAX_PATH] ;
    const int iMenuMax = GetMenuItemCount(hmenuCtx);
    for (int i = 0 ; i < iMenuMax; i++) {
        UINT uiID = GetMenuItemID(hmenuCtx, i) - 1;
        if ((uiID == -1) || (uiID == 0))
            continue;
        res = pCtxMenu->GetCommandString(uiID, GCS_VERBA, NULL, verb, sizeof (verb));
        if (FAILED (res))
            continue;
        if ( stricmp (verb, "undelete") == 0 ) {
            uiCommand = uiID;
            break;
        }
    }
    if ( uiCommand != ~0U ) {
        CMINVOKECOMMANDINFO cmi;
			
        memset(&cmi, 0, sizeof(CMINVOKECOMMANDINFO));
        cmi.cbSize       = sizeof(CMINVOKECOMMANDINFO);
        cmi.lpVerb       = MAKEINTRESOURCEA(uiCommand);
        cmi.lpVerb       = "undelete";
        cmi.nShow        = 0;
        cmi.fMask        = CMIC_MASK_FLAG_NO_UI| CMIC_MASK_ASYNCOK;
        res = pCtxMenu->InvokeCommand((CMINVOKECOMMANDINFO*)&cmi);

        bOk = translateError( res );
        if( bOk )
            finished();
    }
    DestroyMenu(hmenuCtx);
    pCtxMenu->Release();
    ILFree( pidl );
}

void TrashProtocol::rename( const KUrl &oldURL, const KUrl &newURL, KIO::JobFlags flags )
{
    kDebug()<<"TrashProtocol::rename(): old="<<oldURL<<" new="<<newURL<<" overwrite=" << (flags & KIO::Overwrite);

    if (oldURL.protocol() == QLatin1String("trash") && newURL.protocol() == QLatin1String("trash")) {
        error( KIO::ERR_CANNOT_RENAME, oldURL.prettyUrl() );
        return;
    }

    copyOrMove( oldURL, newURL, (flags & KIO::Overwrite), Move );
}

void TrashProtocol::copy( const KUrl &src, const KUrl &dest, int /*permissions*/, KIO::JobFlags flags )
{
    kDebug()<<"TrashProtocol::copy(): " << src << " " << dest;

    if (src.protocol() == QLatin1String("trash") && dest.protocol() == QLatin1String("trash")) {
        error( KIO::ERR_UNSUPPORTED_ACTION, i18n( "This file is already in the trash bin." ) );
        return;
    }

    copyOrMove( src, dest, (flags & KIO::Overwrite), Copy );
}

void TrashProtocol::copyOrMove( const KUrl &src, const KUrl &dest, bool overwrite, CopyOrMove action )
{
    if (src.protocol() == QLatin1String("trash") && dest.isLocalFile()) {
        if ( action == Move ) {
            restore( src, dest );
        } else {
            error( KIO::ERR_UNSUPPORTED_ACTION, i18n( "not supported" ) );
        }
        // Extracting (e.g. via dnd). Ignore original location stored in info file.
        return;
    } else if (src.isLocalFile() && dest.protocol() == QLatin1String("trash")) {
        UINT op = (action == Move) ? FO_DELETE : FO_COPY;
        if( !doFileOp( src, FO_DELETE, FOF_ALLOWUNDO ) )
          return;
        finished();
        return;
    } else {
        error(KIO::ERR_UNSUPPORTED_ACTION, i18n("Internal error in copyOrMove, should never happen"));
    }
}

void TrashProtocol::stat(const KUrl& url)
{
    KIO::UDSEntry entry;
    if( url.path() == QLatin1String("/") ) {
        STRRET strret;
        m_isfDesktop->GetDisplayNameOf( m_iilTrash, SHGDN_NORMAL, &strret );
        entry.insert( KIO::UDSEntry::UDS_NAME,
                      QString::fromUtf16( (const unsigned short*) strret.pOleStr ) );
        entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR );
        entry.insert( KIO::UDSEntry::UDS_ACCESS, 0700 );
        entry.insert( KIO::UDSEntry::UDS_MIME_TYPE, QString::fromLatin1("inode/directory") );
        m_pMalloc->Free( strret.pOleStr );
    } else {
        // TODO: when does this happen?
    }
    statEntry( entry );
    finished();
}

void TrashProtocol::del( const KUrl &url, bool /*isfile*/ )
{
    if( !doFileOp( url, FO_DELETE, 0 ) )
      return;
    finished();
}

void TrashProtocol::listDir(const KUrl& url)
{
    kDebug()<<"TrashProtocol::listDir(): " << url;
    // There are no subfolders in Windows Trash
    listRoot();
}

void TrashProtocol::listRoot()
{
    IEnumIDList *l;
    HRESULT res = m_isfTrashFolder->EnumObjects( 0, SHCONTF_FOLDERS|SHCONTF_NONFOLDERS|SHCONTF_INCLUDEHIDDEN, &l);
    if( res != S_OK )
      return;

    STRRET strret;
    SFGAOF attribs;
    KIO::UDSEntry entry;
    LPITEMIDLIST i;
    WIN32_FIND_DATAW findData;
    while( l->Next( 1, &i, NULL ) == S_OK ) {
      m_isfTrashFolder->GetDisplayNameOf( i, SHGDN_NORMAL, &strret );
      entry.insert( KIO::UDSEntry::UDS_DISPLAY_NAME, 
                    QString::fromUtf16( (const unsigned short*)strret.pOleStr ) );
      m_pMalloc->Free (strret.pOleStr);
      m_isfTrashFolder->GetDisplayNameOf( i, SHGDN_FORPARSING|SHGDN_INFOLDER, &strret );
      entry.insert( KIO::UDSEntry::UDS_NAME, 
                    QString::fromUtf16( (const unsigned short*)strret.pOleStr ) );
      m_pMalloc->Free (strret.pOleStr);
      m_isfTrashFolder->GetAttributesOf( 1, (LPCITEMIDLIST *)&i, &attribs );
      SHGetDataFromIDList( m_isfTrashFolder, i, SHGDFIL_FINDDATA, &findData, sizeof( findData ) );
      entry.insert( KIO::UDSEntry::UDS_SIZE,
                    ((quint64)findData.nFileSizeLow) + (((quint64)findData.nFileSizeHigh) << 32) );
      entry.insert( KIO::UDSEntry::UDS_MODIFICATION_TIME,
                    filetimeToTime_t( &findData.ftLastWriteTime ) );
      entry.insert( KIO::UDSEntry::UDS_ACCESS_TIME,
                    filetimeToTime_t( &findData.ftLastAccessTime ) );
      entry.insert( KIO::UDSEntry::UDS_CREATION_TIME,
                    filetimeToTime_t( &findData.ftCreationTime ) );
      entry.insert( KIO::UDSEntry::UDS_EXTRA,
                    QString::fromUtf16( (const unsigned short*)strret.pOleStr ) );
      entry.insert( KIO::UDSEntry::UDS_EXTRA, QDateTime().toString( Qt::ISODate ) );
      mode_t type = S_IFREG;
      if ( ( attribs & SFGAO_FOLDER ) == SFGAO_FOLDER )
          type = S_IFDIR;
      if ( ( attribs & SFGAO_LINK ) == SFGAO_LINK )
          type = S_IFLNK;
      entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, type );
      mode_t access = 0700;
      if ( ( findData.dwFileAttributes & FILE_ATTRIBUTE_READONLY ) == FILE_ATTRIBUTE_READONLY )
          type = 0300;
      listEntry( entry, false );

      ILFree( i );
    }
    l->Release();
    entry.clear();
    listEntry( entry, true );
    finished();
}

void TrashProtocol::special( const QByteArray & data )
{
    QDataStream stream( data );
    int cmd;
    stream >> cmd;

    switch (cmd) {
    case 1:
        // empty trash folder
//            error( impl.lastErrorCode(), impl.lastErrorMessage() );
        break;
    case 2:
        // convert old trash folder (non-windows only)
        finished();
        break;
    case 3:
    {
        KUrl url;
        stream >> url;
        restore( url, KUrl() );
        break;
    }
    default:
        kWarning(7116) << "Unknown command in special(): " << cmd ;
        error( KIO::ERR_UNSUPPORTED_ACTION, QString::number(cmd) );
        break;
    }
}

void TrashProtocol::put( const KUrl& url, int /*permissions*/, KIO::JobFlags )
{
    kDebug() << "put: " << url;
    // create deleted file. We need to get the mtime and original location from metadata...
    // Maybe we can find the info file for url.fileName(), in case ::rename() was called first, and failed...
    error( KIO::ERR_ACCESS_DENIED, url.prettyUrl() );
}

void TrashProtocol::get( const KUrl& url )
{
    // TODO
}

bool TrashProtocol::doFileOp(const KUrl &url, UINT wFunc, FILEOP_FLAGS fFlags)
{
    // remove first '/' - can't use toLocalFile() because scheme is not file://
    const QString path = url.path().mid(1).replace(QLatin1Char('/'),QLatin1Char('\\'));
    // must be double-null terminated.
    QByteArray delBuf( ( path.length() + 2 ) * 2, 0 );
    memcpy( delBuf.data(), path.utf16(), path.length() * 2 );

    SHFILEOPSTRUCTW op;
    memset( &op, 0, sizeof( SHFILEOPSTRUCTW ) );
    op.wFunc = wFunc;
    op.pFrom = (LPCWSTR)delBuf.constData();
    op.fFlags = fFlags|FOF_NOCONFIRMATION|FOF_NOERRORUI;
    return translateError( SHFileOperationW( &op ) );
}

bool TrashProtocol::translateError(HRESULT hRes)
{
    // TODO!
    if( FAILED( hRes ) ) {
        error( KIO::ERR_DOES_NOT_EXIST, QLatin1String("fixme!") );
        return false;
    }
    return true;
}

#include "kio_trash_win.moc"
