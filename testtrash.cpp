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
    const QString trashDir = QDir::homeDirPath() + "/.Trash/";
    removeFile( trashDir, "info/fileFromHome" );
    removeFile( trashDir, "files/fileFromHome" );
    removeFile( trashDir, "info/fileFromHome_1" );
    removeFile( trashDir, "files/fileFromHome_1" );
    removeFile( trashDir, "info/fileFromOther" );
    removeFile( trashDir, "files/fileFromOther" );
    removeFile( trashDir, "info/symlinkFromHome" );
    removeFile( trashDir, "files/symlinkFromHome" );
    removeFile( trashDir, "info/symlinkFromOther" );
    removeFile( trashDir, "files/symlinkFromOther" );
    removeFile( trashDir, "info/trashDirFromHome" );
    removeFile( trashDir, "files/trashDirFromHome/testfile" );
    removeDir( trashDir, "files/trashDirFromHome" );
    removeFile( trashDir, "info/trashDirFromHome_1" );
    removeFile( trashDir, "files/trashDirFromHome_1/testfile" );
    removeDir( trashDir, "files/trashDirFromHome_1" );
    removeFile( trashDir, "info/trashDirFromOther" );
    removeFile( trashDir, "files/trashDirFromOther/testfile" );
    removeDir( trashDir, "files/trashDirFromOther" );
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

    delRootFile();
    delFileInDirectory();
    delDirectory();

    moveFileFromTrash();
    moveDirectoryFromTrash();
    moveSymlinkFromTrash();
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
    QFile infoFile( info.absFilePath() );
    if ( !infoFile.open( IO_ReadOnly ) )
        kdFatal() << "can't read " << info.absFilePath() << endl;
    QString firstLine;
    infoFile.readLine( firstLine, 100 );
    assert( firstLine == origFilePath + '\n' );
    QString secondLine;
    infoFile.readLine( secondLine, 100 );
    assert( !secondLine.isEmpty() );
    assert( secondLine.endsWith("\n") );
}

static void createTestFile( const QString& path )
{
    QFile f( path );
    if ( !f.open( IO_WriteOnly ) )
        kdFatal() << "Can't create " << path << endl;
    f.writeBlock( "Hello world", 10 );
    f.close();
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
    checkInfoFile( QDir::homeDirPath() + "/.Trash/info/" + fileId, origFilePath );

    QFileInfo files( QDir::homeDirPath() + "/.Trash/files/" + fileId );
    assert( files.isFile() );
    assert( files.size() == 10 );

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
    checkInfoFile( QDir::homeDirPath() + "/.Trash/info/" + fileId, origFilePath );

    QFileInfo files( QDir::homeDirPath() + "/.Trash/files/" + fileId );
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
    kdDebug() << k_funcinfo << endl;
    // setup
    QDir dir;
    bool ok = dir.mkdir( origPath );
    Q_ASSERT( ok );
    createTestFile( origPath + "/testfile" );
    KURL u; u.setPath( origPath );

    // test
    KIO::NetAccess::move( u, "trash:/" );
    checkInfoFile( QDir::homeDirPath() + "/.Trash/info/" + fileId, origPath );

    QFileInfo filesDir( QDir::homeDirPath() + "/.Trash/files/" + fileId );
    assert( filesDir.isDir() );
    QFileInfo files( QDir::homeDirPath() + "/.Trash/files/" + fileId + "/testfile" );
    assert( files.isFile() );
    assert( files.size() == 10 );
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

    QFileInfo file( QDir::homeDirPath() + "/.Trash/files/fileFromHome" );
    assert( !file.exists() );
    QFileInfo info( QDir::homeDirPath() + "/.Trash/info/fileFromHome" );
    assert( !info.exists() );
}

void TestTrash::delFileInDirectory()
{
    kdDebug() << k_funcinfo << endl;

    // test deleting a file inside a trashed directory -> not allowed
    KIO::NetAccess::del( "trash:/0-trashDirFromHome/testfile", 0L );

    QFileInfo dir( QDir::homeDirPath() + "/.Trash/files/trashDirFromHome" );
    assert( dir.exists() );
    QFileInfo file( QDir::homeDirPath() + "/.Trash/files/trashDirFromHome/testfile" );
    assert( file.exists() );
    QFileInfo info( QDir::homeDirPath() + "/.Trash/info/trashDirFromHome" );
    assert( info.exists() );
}

void TestTrash::delDirectory()
{
    kdDebug() << k_funcinfo << endl;

    // test deleting a trashed directory
    KIO::NetAccess::del( "trash:/0-trashDirFromHome", 0L );

    QFileInfo dir( QDir::homeDirPath() + "/.Trash/files/trashDirFromHome" );
    assert( !dir.exists() );
    QFileInfo file( QDir::homeDirPath() + "/.Trash/files/trashDirFromHome/testfile" );
    assert( !file.exists() );
    QFileInfo info( QDir::homeDirPath() + "/.Trash/info/trashDirFromHome" );
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
    assert( item.name() == "/" );
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

void TestTrash::moveFromTrash( const QString& fileId, const QString& destPath )
{
    KURL dest;
    dest.setPath( destPath );

    // A dnd would use move(), but we use moveAs to ensure the final filename
    KIO::Job* job = KIO::moveAs( "trash:/0-" + fileId, dest );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    QString infoFile( QDir::homeDirPath() + "/.Trash/info/" + fileId );
    assert( !QFile::exists( infoFile ) );

    QFileInfo filesItem( QDir::homeDirPath() + "/.Trash/files/" + fileId );
    assert( !filesItem.exists() );

    assert( QFile::exists( destPath ) );
}

void TestTrash::moveFileFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "fileFromOther_restored";
    moveFromTrash( "fileFromOther", destPath );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 10 );
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
