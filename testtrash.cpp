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

#include <config.h>

#include <kurl.h>
#include <kapplication.h>
#include <kio/netaccess.h>
#include <kdebug.h>
#include <kcmdlineargs.h>

#include <qfileinfo.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "kio_trash.h"
#include "testtrash.h"
#include <qdir.h>

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
    // Start with a relatively clean trash too
    const QString trashDir = QDir::homeDirPath() + "/.Trash/";
    dir.remove( trashDir + "info/trashFileFromHome" );
    dir.remove( trashDir + "files/trashFileFromHome" );
    dir.remove( trashDir + "info/trashFileFromOther" );
    dir.remove( trashDir + "files/trashFileFromOther" );
    dir.remove( trashDir + "files/trashDirFromHome/" );
    dir.remove( trashDir + "files/trashDirFromHome/testfile" );
    dir.remove( trashDir + "files/trashDirFromOther/" );
    dir.remove( trashDir + "files/trashDirFromOther/testfile" );
}

void TestTrash::runAll()
{
    urlTestFile();
    urlTestDirectory();
    urlTestSubDirectory();
    trashFileFromHome();
    /////////trashFileFromOther(); TODO
    tryRenameInsideTrash();
    trashDirectoryFromHome();
    trashDirectoryFromOther();
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

void TestTrash::trashFileFromHome()
{
    kdDebug() << k_funcinfo << endl;
    // setup
    QString origFilePath = homeTmpDir() + "trashFileFromHome";
    createTestFile( origFilePath );
    KURL u;
    u.setPath( origFilePath );

    // test
    KIO::NetAccess::move( u, "trash:/" );
    checkInfoFile( QDir::homeDirPath() + "/.Trash/info/trashFileFromHome", origFilePath );

    QFileInfo files( QDir::homeDirPath() + "/.Trash/files/trashFileFromHome" );
    assert( files.isFile() );
    assert( files.size() == 10 );

    // coolo suggests testing that the original file is actually gone, too :)
    assert( !QFile::exists( origFilePath ) );
}

void TestTrash::trashFileFromOther()
{
    kdDebug() << k_funcinfo << endl;
    // setup
    QString origFilePath = otherTmpDir() + "trashFileFromOther";
    createTestFile( origFilePath );
    KURL u;
    u.setPath( origFilePath );

    // test
    KIO::NetAccess::move( u, "trash:/" );
    checkInfoFile( QDir::homeDirPath() + "/.Trash/info/trashFileFromOther", origFilePath );

    QFileInfo files( QDir::homeDirPath() + "/.Trash/files/trashFileFromOther" );
    assert( files.isFile() );
    assert( files.size() == 10 );

    // coolo suggests testing that the original file is actually gone, too :)
    assert( !QFile::exists( origFilePath ) );
}

void TestTrash::tryRenameInsideTrash()
{
    kdDebug() << k_funcinfo << endl;
    // Can't use NetAccess::move(), it brings up SkipDlg.
    bool worked = KIO::NetAccess::file_move( "trash:/tryRenameInsideTrash", "trash:/foobar" );
    assert( !worked );
}

void TestTrash::trashDirectoryFromHome()
{
    kdDebug() << k_funcinfo << endl;
    // setup
    QString origPath = homeTmpDir() + "trashDirFromHome";
    QDir dir;
    bool ok = dir.mkdir( origPath );
    Q_ASSERT( ok );
    createTestFile( origPath + "/testfile" );
    KURL u; u.setPath( origPath );

    // test
    KIO::NetAccess::move( u, "trash:/" );
    checkInfoFile( QDir::homeDirPath() + "/.Trash/info/trashDirFromHome", origPath );

    QFileInfo filesDir( QDir::homeDirPath() + "/.Trash/files/trashDirFromHome" );
    assert( filesDir.isDir() );
    QFileInfo files( QDir::homeDirPath() + "/.Trash/files/trashDirFromHome/testfile" );
    assert( files.isFile() );
    assert( files.size() == 10 );
    assert( !QFile::exists( origPath ) );
}

void TestTrash::trashDirectoryFromOther()
{

}
