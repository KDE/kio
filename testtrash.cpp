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
#include "testtrash.h"

#include <config.h>

#include <kurl.h>
#include <kapplication.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <kdebug.h>
#include <kcmdlineargs.h>

#include <qdir.h>
#include <qfileinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <kfileitem.h>
#include <kstandarddirs.h>

static bool check(const QString& txt, QString a, QString b)
{
    if (a.isEmpty())
        a = QString::null;
    if (b.isEmpty())
        b = QString::null;
    if (a == b) {
        kdDebug() << txt << " : checking '" << a << "' against expected value '" << b << "'... " << "ok" << endl;
    }
    else {
        kdDebug() << txt << " : checking '" << a << "' against expected value '" << b << "'... " << "KO !" << endl;
        exit(1);
    }
    return true;
}

int main(int argc, char *argv[])
{
    KApplication::disableAutoDcopRegistration();
    KCmdLineArgs::init(argc,argv,"testtrash", 0, 0, 0, 0);
    KApplication app;

    TestTrash test;
    test.setup();
    test.runAll();
    kdDebug() << "All tests OK." << endl;
    return 0; // success. The exit(1) in check() is what happens in case of failure.
}

QString TestTrash::homeTmpDir() const
{
    return QDir::homeDirPath() + "/.kde/testtrash/";
}

QString TestTrash::otherTmpDir() const
{
    // This one needs to be on another partition
    return "/tmp/testtrash/";
}

void TestTrash::setup()
{
    m_trashDir = KGlobal::dirs()->localxdgdatadir() + "Trash";

    // Start with a clean base dir
    KIO::NetAccess::del( homeTmpDir(), 0 );
    KIO::NetAccess::del( otherTmpDir(), 0 );
    QDir dir; // TT: why not a static method?
    bool ok = dir.mkdir( homeTmpDir() );
    if ( !ok )
        kdFatal() << "Couldn't create " << homeTmpDir() << endl;
    ok = dir.mkdir( otherTmpDir() );
    if ( !ok )
        kdFatal() << "Couldn't create " << otherTmpDir() << endl;
    cleanTrash();
}

static void removeFile( const QString& trashDir, const QString& fileName )
{
    QDir dir;
    dir.remove( trashDir + fileName );
    assert( !QDir( trashDir + fileName ).exists() );
}

static void removeDir( const QString& trashDir, const QString& dirName )
{
    QDir dir;
    dir.rmdir( trashDir + dirName );
    assert( !QDir( trashDir + dirName ).exists() );
}

void TestTrash::cleanTrash()
{
    // Start with a relatively clean trash too
    removeFile( m_trashDir, "/info/fileFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromHome" );
    removeFile( m_trashDir, "/info/fileFromHome_1.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromHome_1" );
    removeFile( m_trashDir, "/info/fileFromOther.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromOther" );
    removeFile( m_trashDir, "/info/symlinkFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/symlinkFromHome" );
    removeFile( m_trashDir, "/info/symlinkFromOther.trashinfo" );
    removeFile( m_trashDir, "/files/symlinkFromOther" );
    removeFile( m_trashDir, "/info/trashDirFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/trashDirFromHome/testfile" );
    removeDir( m_trashDir, "/files/trashDirFromHome" );
    removeFile( m_trashDir, "/info/trashDirFromHome_1.trashinfo" );
    removeFile( m_trashDir, "/files/trashDirFromHome_1/testfile" );
    removeDir( m_trashDir, "/files/trashDirFromHome_1" );
    removeFile( m_trashDir, "/info/trashDirFromOther.trashinfo" );
    removeFile( m_trashDir, "/files/trashDirFromOther/testfile" );
    removeDir( m_trashDir, "/files/trashDirFromOther" );

    //system( "find ~/.local/share/Trash" );
}

void TestTrash::runAll()
{
    urlTestFile();
    urlTestDirectory();
    urlTestSubDirectory();

    trashFileFromHome();
    trashFileFromOther();
    trashSymlinkFromHome();
    trashSymlinkFromOther();
    trashDirectoryFromHome();
    trashDirectoryFromOther();

    tryRenameInsideTrash();

    statRoot();
    statFileInRoot();
    statDirectoryInRoot();
    statSymlinkInRoot();
    statFileInDirectory();

    copyFileFromTrash();
    // To test case of already-existing destination, uncomment this.
    // This brings up the "rename" dialog though, so it can't be fully automated
    //copyFileFromTrash();
    copyFileInDirectoryFromTrash();
    copyDirectoryFromTrash();
    copySymlinkFromTrash();

    moveFileFromTrash();
    moveFileInDirectoryFromTrash();
    moveDirectoryFromTrash();
    moveSymlinkFromTrash();

    listRootDir();

    delRootFile();
    delFileInDirectory();
    delDirectory();

    getFile();
    restoreFile();

    // Not possible to test here:
    // - emptying the trash (test it with "kemptytrash")
    // - initial trash creation
    // - trash migration
    // - the updating of the trash icon on the desktop
}

void TestTrash::urlTestFile()
{
    QString url = TrashProtocol::makeURL( 1, "fileId", QString::null );
    check( "makeURL for a file", url, "trash:/1-fileId" );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashProtocol::parseURL( KURL( url ), trashId, fileId, relativePath );
    assert( ok );
    check( "parseURL: trashId", QString::number( trashId ), "1" );
    check( "parseURL: fileId", fileId, "fileId" );
    check( "parseURL: relativePath", relativePath, QString::null );
}

void TestTrash::urlTestDirectory()
{
    QString url = TrashProtocol::makeURL( 1, "fileId", "subfile" );
    check( "makeURL", url, "trash:/1-fileId/subfile" );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashProtocol::parseURL( KURL( url ), trashId, fileId, relativePath );
    assert( ok );
    check( "parseURL: trashId", QString::number( trashId ), "1" );
    check( "parseURL: fileId", fileId, "fileId" );
    check( "parseURL: relativePath", relativePath, "subfile" );
}

void TestTrash::urlTestSubDirectory()
{
    QString url = TrashProtocol::makeURL( 1, "fileId", "subfile/foobar" );
    check( "makeURL", url, "trash:/1-fileId/subfile/foobar" );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashProtocol::parseURL( KURL( url ), trashId, fileId, relativePath );
    assert( ok );
    check( "parseURL: trashId", QString::number( trashId ), "1" );
    check( "parseURL: fileId", fileId, "fileId" );
    check( "parseURL: relativePath", relativePath, "subfile/foobar" );
}

static void checkInfoFile( const QString& infoPath, const QString& origFilePath )
{
    QFileInfo info( infoPath );
    assert( info.isFile() );
    assert( info.exists() );
    KSimpleConfig infoFile( info.absFilePath(), true );
    if ( !infoFile.hasGroup( "Trash Info" ) )
        kdFatal() << "no Trash Info group in " << info.absFilePath() << endl;
    infoFile.setGroup( "Trash Info" );
    const QString origPath = infoFile.readEntry( "Path" );
    assert( !origPath.isEmpty() );
    assert( origPath == origFilePath );
    const QString date = infoFile.readEntry( "DeletionDate" );
    assert( !date.isEmpty() );
    assert( date.contains( "T" ) );
}

static void createTestFile( const QString& path )
{
    QFile f( path );
    if ( !f.open( IO_WriteOnly ) )
        kdFatal() << "Can't create " << path << endl;
    f.writeBlock( "Hello world\n", 12 );
    f.close();
    assert( QFile::exists( path ) );
}

void TestTrash::trashFile( const QString& origFilePath, const QString& fileId )
{
    // setup
    createTestFile( origFilePath );
    KURL u;
    u.setPath( origFilePath );

    // test
    bool ok = KIO::NetAccess::move( u, "trash:/" );
    assert( ok );
    checkInfoFile( m_trashDir + "/info/" + fileId + ".trashinfo", origFilePath );

    QFileInfo files( m_trashDir + "/files/" + fileId );
    assert( files.isFile() );
    assert( files.size() == 12 );

    // coolo suggests testing that the original file is actually gone, too :)
    assert( !QFile::exists( origFilePath ) );
}

void TestTrash::trashFileFromHome()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );

    // Do it again, check that we got a different id
    trashFile( homeTmpDir() + fileName, fileName + "_1" );
}

void TestTrash::trashFileFromOther()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "fileFromOther";
    trashFile( otherTmpDir() + fileName, fileName );
}

void TestTrash::trashSymlink( const QString& origFilePath, const QString& fileId )
{
    kdDebug() << k_funcinfo << endl;
    // setup
    const char* target = "/tmp";
    bool ok = ::symlink( target, QFile::encodeName( origFilePath ) ) == 0;
    assert( ok );
    KURL u;
    u.setPath( origFilePath );

    // test
    ok = KIO::NetAccess::move( u, "trash:/" );
    assert( ok );
    checkInfoFile( m_trashDir + "/info/" + fileId + ".trashinfo", origFilePath );

    QFileInfo files( m_trashDir + "/files/" + fileId );
    assert( files.isSymLink() );
    assert( files.readLink() == QFile::decodeName( target ) );
    assert( !QFile::exists( origFilePath ) );
}

void TestTrash::trashSymlinkFromHome()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "symlinkFromHome";
    trashSymlink( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashSymlinkFromOther()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "symlinkFromOther";
    trashSymlink( otherTmpDir() + fileName, fileName );
}

void TestTrash::trashDirectory( const QString& origPath, const QString& fileId )
{
    kdDebug() << k_funcinfo << fileId << endl;
    // setup
    QDir dir;
    bool ok = dir.mkdir( origPath );
    Q_ASSERT( ok );
    createTestFile( origPath + "/testfile" );
    KURL u; u.setPath( origPath );

    // test
    ok = KIO::NetAccess::move( u, "trash:/" );
    assert( ok );
    checkInfoFile( m_trashDir + "/info/" + fileId + ".trashinfo", origPath );

    QFileInfo filesDir( m_trashDir + "/files/" + fileId );
    assert( filesDir.isDir() );
    QFileInfo files( m_trashDir + "/files/" + fileId + "/testfile" );
    assert( files.exists() );
    assert( files.isFile() );
    assert( files.size() == 12 );
    assert( !QFile::exists( origPath ) );
}

void TestTrash::trashDirectoryFromHome()
{
    kdDebug() << k_funcinfo << endl;
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
    // Do it again, check that we got a different id
    trashDirectory( homeTmpDir() + dirName, dirName + "_1" );
}

void TestTrash::trashDirectoryFromOther()
{
    kdDebug() << k_funcinfo << endl;
    QString dirName = "trashDirFromOther";
    trashDirectory( otherTmpDir() + dirName, dirName );
}

void TestTrash::tryRenameInsideTrash()
{
    kdDebug() << k_funcinfo << endl;
    // Can't use NetAccess::move(), it brings up SkipDlg.
    bool worked = KIO::NetAccess::file_move( "trash:/0-tryRenameInsideTrash", "trash:/foobar" );
    assert( !worked );
}

void TestTrash::delRootFile()
{
    kdDebug() << k_funcinfo << endl;

    // test deleting a trashed file
    KIO::NetAccess::del( "trash:/0-fileFromHome", 0L );

    QFileInfo file( m_trashDir + "/files/fileFromHome" );
    assert( !file.exists() );
    QFileInfo info( m_trashDir + "/info/fileFromHome.trashinfo" );
    assert( !info.exists() );
}

void TestTrash::delFileInDirectory()
{
    kdDebug() << k_funcinfo << endl;

    // test deleting a file inside a trashed directory -> not allowed
    KIO::NetAccess::del( "trash:/0-trashDirFromHome/testfile", 0L );

    QFileInfo dir( m_trashDir + "/files/trashDirFromHome" );
    assert( dir.exists() );
    QFileInfo file( m_trashDir + "/files/trashDirFromHome/testfile" );
    assert( file.exists() );
    QFileInfo info( m_trashDir + "/info/trashDirFromHome.trashinfo" );
    assert( info.exists() );
}

void TestTrash::delDirectory()
{
    kdDebug() << k_funcinfo << endl;

    // test deleting a trashed directory
    KIO::NetAccess::del( "trash:/0-trashDirFromHome", 0L );

    QFileInfo dir( m_trashDir + "/files/trashDirFromHome" );
    assert( !dir.exists() );
    QFileInfo file( m_trashDir + "/files/trashDirFromHome/testfile" );
    assert( !file.exists() );
    QFileInfo info( m_trashDir + "/info/trashDirFromHome.trashinfo" );
    assert( !info.exists() );
}

void TestTrash::statRoot()
{
    kdDebug() << k_funcinfo << endl;
    KURL url( "trash:/" );
    KIO::UDSEntry entry;
    bool ok = KIO::NetAccess::stat( url, entry, 0 );
    assert( ok );
    KFileItem item( entry, url );
    assert( item.isDir() );
    assert( !item.isLink() );
    assert( item.isReadable() );
    assert( item.isWritable() );
    assert( !item.isHidden() );
    assert( item.name() == "." );
    assert( item.acceptsDrops() );
}

void TestTrash::statFileInRoot()
{
    kdDebug() << k_funcinfo << endl;
    KURL url( "trash:/0-fileFromOther" );
    KIO::UDSEntry entry;
    bool ok = KIO::NetAccess::stat( url, entry, 0 );
    assert( ok );
    KFileItem item( entry, url );
    assert( item.isFile() );
    assert( !item.isLink() );
    assert( item.isReadable() );
    assert( !item.isWritable() );
    assert( !item.isHidden() );
    assert( item.name() == "fileFromOther" );
    assert( !item.acceptsDrops() );
}

void TestTrash::statDirectoryInRoot()
{
    kdDebug() << k_funcinfo << endl;
    KURL url( "trash:/0-trashDirFromHome" );
    KIO::UDSEntry entry;
    bool ok = KIO::NetAccess::stat( url, entry, 0 );
    assert( ok );
    KFileItem item( entry, url );
    assert( item.isDir() );
    assert( !item.isLink() );
    assert( item.isReadable() );
    assert( !item.isWritable() );
    assert( !item.isHidden() );
    assert( item.name() == "trashDirFromHome" );
    assert( !item.acceptsDrops() );
}

void TestTrash::statSymlinkInRoot()
{
    kdDebug() << k_funcinfo << endl;
    KURL url( "trash:/0-symlinkFromOther" );
    KIO::UDSEntry entry;
    bool ok = KIO::NetAccess::stat( url, entry, 0 );
    assert( ok );
    KFileItem item( entry, url );
    assert( item.isLink() );
    assert( item.linkDest() == "/tmp" );
    assert( item.isReadable() );
    assert( !item.isWritable() );
    assert( !item.isHidden() );
    assert( item.name() == "symlinkFromOther" );
    assert( !item.acceptsDrops() );
}

void TestTrash::statFileInDirectory()
{
    kdDebug() << k_funcinfo << endl;
    KURL url( "trash:/0-trashDirFromHome/testfile" );
    KIO::UDSEntry entry;
    bool ok = KIO::NetAccess::stat( url, entry, 0 );
    assert( ok );
    KFileItem item( entry, url );
    assert( item.isFile() );
    assert( !item.isLink() );
    assert( item.isReadable() );
    assert( !item.isWritable() );
    assert( !item.isHidden() );
    assert( item.name() == "testfile" );
    assert( !item.acceptsDrops() );
}

void TestTrash::copyFromTrash( const QString& fileId, const QString& destPath, const QString& relativePath )
{
    KURL src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KURL dest;
    dest.setPath( destPath );

    // A dnd would use copy(), but we use copyAs to ensure the final filename
    //kdDebug() << k_funcinfo << "copyAs:" << src << " -> " << dest << endl;
    KIO::Job* job = KIO::copyAs( src, dest );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    assert( QFile::exists( infoFile ) );

    QFileInfo filesItem( m_trashDir + "/files/" + fileId );
    assert( filesItem.exists() );

    assert( QFile::exists( destPath ) );
}

void TestTrash::copyFileFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "fileFromOther_copied";
    copyFromTrash( "fileFromOther", destPath );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::copyFileInDirectoryFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "testfile_copied";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::copyDirectoryFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "trashDirFromOther_copied";
    copyFromTrash( "trashDirFromOther", destPath );
    assert( QFileInfo( destPath ).isDir() );
}

void TestTrash::copySymlinkFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "symlinkFromOther_copied";
    copyFromTrash( "symlinkFromOther", destPath );
    assert( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::moveFromTrash( const QString& fileId, const QString& destPath, const QString& relativePath )
{
    KURL src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KURL dest;
    dest.setPath( destPath );

    // A dnd would use move(), but we use moveAs to ensure the final filename
    KIO::Job* job = KIO::moveAs( src, dest );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    assert( !QFile::exists( infoFile ) );

    QFileInfo filesItem( m_trashDir + "/files/" + fileId );
    assert( !filesItem.exists() );

    assert( QFile::exists( destPath ) );
}

void TestTrash::moveFileFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "fileFromOther_restored";
    moveFromTrash( "fileFromOther", destPath );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::moveFileInDirectoryFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "testfile_restored";
    copyFromTrash( "trashDirFromOther", destPath, "testfile" );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::moveDirectoryFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "trashDirFromOther_restored";
    moveFromTrash( "trashDirFromOther", destPath );
    assert( QFileInfo( destPath ).isDir() );
}

void TestTrash::moveSymlinkFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "symlinkFromOther_restored";
    moveFromTrash( "symlinkFromOther", destPath );
    assert( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::getFile()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileId = "fileFromHome_1";
    const KURL url = TrashProtocol::makeURL( 0, fileId, QString::null );
    QString tmpFile;
    bool ok = KIO::NetAccess::download( url, tmpFile, 0 );
    assert( ok );
    QFile file( tmpFile );
    ok = file.open( IO_ReadOnly );
    assert( ok );
    QByteArray str = file.readAll();
    QCString cstr( str.data(), str.size() + 1 );
    if ( cstr != "Hello world\n" )
        kdFatal() << "get() returned the following data:" << cstr << endl;
    file.close();
    KIO::NetAccess::removeTempFile( tmpFile );
}

void TestTrash::restoreFile()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileId = "fileFromHome_1";
    const KURL url = TrashProtocol::makeURL( 0, fileId, QString::null );
    const QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    const QString filesItem( m_trashDir + "/files/" + fileId );

    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( packedArgs, IO_WriteOnly );
    stream << (int)3 << url;
    KIO::Job* job = KIO::special( url, packedArgs );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );

    assert( !QFile::exists( infoFile ) );
    assert( !QFile::exists( filesItem ) );

    const QString destPath = homeTmpDir() + "fileFromHome";
    assert( QFile::exists( destPath ) );
}

void TestTrash::listRootDir()
{
    kdDebug() << k_funcinfo << endl;
    m_entryCount = 0;
    KIO::ListJob* job = KIO::listDir( "trash:/" );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kdDebug() << "listDir done - m_entryCount=" << m_entryCount << endl;
    assert( m_entryCount > 1 );
}

void TestTrash::slotEntries( KIO::Job*, const KIO::UDSEntryList& lst )
{
    bool dotFound = false;

    for( KIO::UDSEntryList::ConstIterator it = lst.begin(); it != lst.end(); ++it ) {
        KIO::UDSEntry::ConstIterator it2 = (*it).begin();
        QString displayName;
        KURL url;
        for( ; it2 != (*it).end(); it2++ ) {
            switch ((*it2).m_uds) {
            case KIO::UDS_NAME:
                displayName = (*it2).m_str;
                break;
            case KIO::UDS_URL:
                url = (*it2).m_str;
                break;
            }
        }
        if ( displayName == "." ) {
            assert( !dotFound ); // only once
            dotFound = true;
        } else {
            assert( url.protocol() == "trash" );
        }
    }
    assert( dotFound );
    m_entryCount += lst.count();
}

#include "testtrash.moc"
