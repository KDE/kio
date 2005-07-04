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
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

// Get those asserts to work
#undef NDEBUG
#undef NO_DEBUG

#include "kio_trash.h"
#include "testtrash.h"

#include <config.h>

#include <kurl.h>
#include <klocale.h>
#include <kapplication.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <kdebug.h>
#include <kcmdlineargs.h>

#include <qdir.h>
#include <qfileinfo.h>
#include <qvaluevector.h>

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

// There are two ways to test encoding things:
// * with utf8 filenames
// * with latin1 filenames
//
//#define UTF8TEST 1

int main(int argc, char *argv[])
{
    // Ensure a known QFile::encodeName behavior for trashUtf8FileFromHome
    // However this assume your $HOME doesn't use characters from other locales...
    setenv( "LC_ALL", "en_GB.ISO-8859-1", 1 );
#ifdef UTF8TEST
    setenv( "KDE_UTF8_FILENAMES", "true", 1 );
#else
    unsetenv( "KDE_UTF8_FILENAMES" );
#endif

    // Use another directory than the real one, just to keep things clean
    setenv( "XDG_DATA_HOME", QFile::encodeName( QDir::homeDirPath() + "/.local-testtrash" ), true );
    setenv( "KDE_FORK_SLAVES", "yes", true );

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

QString TestTrash::utf8FileName() const
{
    return QString( "test" ) + QChar( 0x2153 ); // "1/3" character, not part of latin1
}

QString TestTrash::umlautFileName() const
{
    return QString( "umlaut" ) + QChar( 0xEB );
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

void TestTrash::setup()
{
    m_trashDir = KGlobal::dirs()->localxdgdatadir() + "Trash";
    kdDebug() << "setup: using trash directory " << m_trashDir << endl;

    // Look for another writable partition than $HOME (not mandatory)
    TrashImpl impl;
    impl.init();

    TrashImpl::TrashDirMap trashDirs = impl.trashDirectories();
    TrashImpl::TrashDirMap topDirs = impl.topDirectories();
    bool foundTrashDir = false;
    m_otherPartitionId = 0;
    m_tmpIsWritablePartition = false;
    m_tmpTrashId = -1;
    QValueVector<int> writableTopDirs;
    for ( TrashImpl::TrashDirMap::ConstIterator it = trashDirs.begin(); it != trashDirs.end() ; ++it ) {
        if ( it.key() == 0 ) {
            assert( it.data() == m_trashDir );
            assert( topDirs.find( 0 ) == topDirs.end() );
            foundTrashDir = true;
        } else {
            assert( topDirs.find( it.key() ) != topDirs.end() );
            const QString topdir = topDirs[it.key()];
            if ( QFileInfo( topdir ).isWritable() ) {
                writableTopDirs.append( it.key() );
                if ( topdir == "/tmp/" ) {
                    m_tmpIsWritablePartition = true;
                    m_tmpTrashId = it.key();
                    kdDebug() << "/tmp is on its own partition (trashid=" << m_tmpTrashId << "), some tests will be skipped" << endl;
                    removeFile( it.data(), "/info/fileFromOther.trashinfo" );
                    removeFile( it.data(), "/files/fileFromOther" );
                    removeFile( it.data(), "/info/symlinkFromOther.trashinfo" );
                    removeFile( it.data(), "/files/symlinkFromOther" );
                    removeFile( it.data(), "/info/trashDirFromOther.trashinfo" );
                    removeFile( it.data(), "/files/trashDirFromOther/testfile" );
                    removeDir( it.data(), "/files/trashDirFromOther" );
                }
            }
        }
    }
    for ( QValueVector<int>::const_iterator it = writableTopDirs.begin(); it != writableTopDirs.end(); ++it ) {
        const QString topdir = topDirs[ *it ];
        const QString trashdir = trashDirs[ *it ];
        assert( !topdir.isEmpty() );
        assert( !trashDirs.isEmpty() );
        if ( topdir != "/tmp/" ||         // we'd prefer not to use /tmp here, to separate the tests
               ( writableTopDirs.count() > 1 ) ) // but well, if we have no choice, take it
        {
            m_otherPartitionTopDir = topdir;
            m_otherPartitionTrashDir = trashdir;
            m_otherPartitionId = *it;
            kdDebug() << "OK, found another writable partition: topDir=" << m_otherPartitionTopDir
                      << " trashDir=" << m_otherPartitionTrashDir << " id=" << m_otherPartitionId << endl;
            break;
        }
    }
    // Check that m_trashDir got listed
    assert( foundTrashDir );
    if ( m_otherPartitionTrashDir.isEmpty() )
        kdWarning() << "No writable partition other than $HOME found, some tests will be skipped" << endl;

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


void TestTrash::cleanTrash()
{
    // Start with a relatively clean trash too
    removeFile( m_trashDir, "/info/fileFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromHome" );
    removeFile( m_trashDir, "/info/fileFromHome_1.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromHome_1" );
    removeFile( m_trashDir, "/info/file%2f.trashinfo" );
    removeFile( m_trashDir, "/files/file%2f" );
    removeFile( m_trashDir, "/info/" + utf8FileName() + ".trashinfo" );
    removeFile( m_trashDir, "/files/" + utf8FileName() );
    removeFile( m_trashDir, "/info/" + umlautFileName() + ".trashinfo" );
    removeFile( m_trashDir, "/files/" + umlautFileName() );
    removeFile( m_trashDir, "/info/fileFromOther.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromOther" );
    removeFile( m_trashDir, "/info/symlinkFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/symlinkFromHome" );
    removeFile( m_trashDir, "/info/symlinkFromOther.trashinfo" );
    removeFile( m_trashDir, "/files/symlinkFromOther" );
    removeFile( m_trashDir, "/info/brokenSymlinkFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/brokenSymlinkFromHome" );
    removeFile( m_trashDir, "/info/trashDirFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/trashDirFromHome/testfile" );
    removeDir( m_trashDir, "/files/trashDirFromHome" );
    removeFile( m_trashDir, "/info/trashDirFromHome_1.trashinfo" );
    removeFile( m_trashDir, "/files/trashDirFromHome_1/testfile" );
    removeDir( m_trashDir, "/files/trashDirFromHome_1" );
    removeFile( m_trashDir, "/info/trashDirFromOther.trashinfo" );
    removeFile( m_trashDir, "/files/trashDirFromOther/testfile" );
    removeDir( m_trashDir, "/files/trashDirFromOther" );
    // for trashDirectoryOwnedByRoot
    KIO::NetAccess::del( m_trashDir + "/files/cups", 0 );
    KIO::NetAccess::del( m_trashDir + "/files/boot", 0 );
    KIO::NetAccess::del( m_trashDir + "/files/etc", 0 );

    //system( "find ~/.local-testtrash/share/Trash" );
}

void TestTrash::runAll()
{
    urlTestFile();
    urlTestDirectory();
    urlTestSubDirectory();

    trashFileFromHome();
    trashPercentFileFromHome();
#ifdef UTF8TEST
    trashUtf8FileFromHome();
#endif
    trashUmlautFileFromHome();
    testTrashNotEmpty();
    trashFileFromOther();
    trashFileIntoOtherPartition();
    trashFileOwnedByRoot();
    trashSymlinkFromHome();
    trashSymlinkFromOther();
    trashBrokenSymlinkFromHome();
    trashDirectoryFromHome();
    trashDirectoryFromOther();
    trashDirectoryOwnedByRoot();

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
    listRecursiveRootDir();
    listSubDir();

    delRootFile();
    delFileInDirectory();
    delDirectory();

    getFile();
    restoreFile();
    restoreFileFromSubDir();
    restoreFileToDeletedDirectory();

    emptyTrash();

    // TODO: test
    // - trash migration
    // - the actual updating of the trash icon on the desktop
}

void TestTrash::urlTestFile()
{
    const KURL url = TrashImpl::makeURL( 1, "fileId", QString::null );
    check( "makeURL for a file", url.url(), "trash:/1-fileId" );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    assert( ok );
    check( "parseURL: trashId", QString::number( trashId ), "1" );
    check( "parseURL: fileId", fileId, "fileId" );
    check( "parseURL: relativePath", relativePath, QString::null );
}

void TestTrash::urlTestDirectory()
{
    const KURL url = TrashImpl::makeURL( 1, "fileId", "subfile" );
    check( "makeURL", url.url(), "trash:/1-fileId/subfile" );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    assert( ok );
    check( "parseURL: trashId", QString::number( trashId ), "1" );
    check( "parseURL: fileId", fileId, "fileId" );
    check( "parseURL: relativePath", relativePath, "subfile" );
}

void TestTrash::urlTestSubDirectory()
{
    const KURL url = TrashImpl::makeURL( 1, "fileId", "subfile/foobar" );
    check( "makeURL", url.url(), "trash:/1-fileId/subfile/foobar" );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    assert( ok );
    check( "parseURL: trashId", QString::number( trashId ), "1" );
    check( "parseURL: fileId", fileId, "fileId" );
    check( "parseURL: relativePath", relativePath, "subfile/foobar" );
}

static void checkInfoFile( const QString& infoPath, const QString& origFilePath )
{
    kdDebug() << k_funcinfo << infoPath << endl;
    QFileInfo info( infoPath );
    assert( info.exists() );
    assert( info.isFile() );
    KSimpleConfig infoFile( info.absFilePath(), true );
    if ( !infoFile.hasGroup( "Trash Info" ) )
        kdFatal() << "no Trash Info group in " << info.absFilePath() << endl;
    infoFile.setGroup( "Trash Info" );
    const QString origPath = infoFile.readEntry( "Path" );
    assert( !origPath.isEmpty() );
    assert( origPath == KURL::encode_string( origFilePath, KGlobal::locale()->fileEncodingMib() ) );
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
    if ( !QFile::exists( origFilePath ) )
        createTestFile( origFilePath );
    KURL u;
    u.setPath( origFilePath );

    // test
    KIO::Job* job = KIO::move( u, "trash:/" );
    QMap<QString, QString> metaData;
    //bool ok = KIO::NetAccess::move( u, "trash:/" );
    bool ok = KIO::NetAccess::synchronousRun( job, 0, 0, 0, &metaData );
    if ( !ok )
        kdError() << "moving " << u << " to trash failed with error " << KIO::NetAccess::lastError() << " " << KIO::NetAccess::lastErrorString() << endl;
    assert( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kdDebug() << " TESTS SKIPPED" << endl;
    } else {
        checkInfoFile( m_trashDir + "/info/" + fileId + ".trashinfo", origFilePath );

        QFileInfo files( m_trashDir + "/files/" + fileId );
        assert( files.isFile() );
        assert( files.size() == 12 );
    }

    // coolo suggests testing that the original file is actually gone, too :)
    assert( !QFile::exists( origFilePath ) );

    assert( !metaData.isEmpty() );
    bool found = false;
    QMap<QString, QString>::ConstIterator it = metaData.begin();
    for ( ; it != metaData.end() ; ++it ) {
        if ( it.key().startsWith( "trashURL" ) ) {
            const QString origPath = it.key().mid( 9 );
            KURL trashURL( it.data() );
            kdDebug() << trashURL << endl;
            assert( !trashURL.isEmpty() );
            assert( trashURL.protocol() == "trash" );
            int trashId = 0;
            if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition )
                trashId = m_tmpTrashId;
            assert( trashURL.path() == "/" + QString::number( trashId ) + "-" + fileId );
            found = true;
        }
    }
    assert( found );
}

void TestTrash::trashFileFromHome()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );

    // Do it again, check that we got a different id
    trashFile( homeTmpDir() + fileName, fileName + "_1" );
}

void TestTrash::trashPercentFileFromHome()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "file%2f";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashUtf8FileFromHome()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = utf8FileName();
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashUmlautFileFromHome()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = umlautFileName();
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::testTrashNotEmpty()
{
    KSimpleConfig cfg( "trashrc", true );
    assert( cfg.hasGroup( "Status" ) );
    cfg.setGroup( "Status" );
    assert( cfg.readBoolEntry( "Empty", true ) == false );
}

void TestTrash::trashFileFromOther()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "fileFromOther";
    trashFile( otherTmpDir() + fileName, fileName );
}

void TestTrash::trashFileIntoOtherPartition()
{
    if ( m_otherPartitionTrashDir.isEmpty() ) {
        kdDebug() << k_funcinfo << " - SKIPPED" << endl;
        return;
    }
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "testtrash-file";
    const QString origFilePath = m_otherPartitionTopDir + fileName;
    const QString fileId = fileName;
    // cleanup
    QFile::remove( m_otherPartitionTrashDir + "/info/" + fileId + ".trashinfo" );
    QFile::remove( m_otherPartitionTrashDir + "/files/" + fileId );

    // setup
    if ( !QFile::exists( origFilePath ) )
        createTestFile( origFilePath );
    KURL u;
    u.setPath( origFilePath );

    // test
    KIO::Job* job = KIO::move( u, "trash:/" );
    QMap<QString, QString> metaData;
    bool ok = KIO::NetAccess::synchronousRun( job, 0, 0, 0, &metaData );
    assert( ok );
    // Note that the Path stored in the info file is relative, on other partitions (#95652)
    checkInfoFile( m_otherPartitionTrashDir + "/info/" + fileId + ".trashinfo", fileName );

    QFileInfo files( m_otherPartitionTrashDir + "/files/" + fileId );
    assert( files.isFile() );
    assert( files.size() == 12 );

    // coolo suggests testing that the original file is actually gone, too :)
    assert( !QFile::exists( origFilePath ) );

    assert( !metaData.isEmpty() );
    bool found = false;
    QMap<QString, QString>::ConstIterator it = metaData.begin();
    for ( ; it != metaData.end() ; ++it ) {
        if ( it.key().startsWith( "trashURL" ) ) {
            const QString origPath = it.key().mid( 9 );
            KURL trashURL( it.data() );
            kdDebug() << trashURL << endl;
            assert( !trashURL.isEmpty() );
            assert( trashURL.protocol() == "trash" );
            assert( trashURL.path() == QString( "/%1-%2" ).arg( m_otherPartitionId ).arg( fileId ) );
            found = true;
        }
    }
    assert( found );
}

void TestTrash::trashFileOwnedByRoot()
{
    kdDebug() << k_funcinfo << endl;
    KURL u;
    u.setPath( "/etc/passwd" );
    const QString fileId = "passwd";

    KIO::CopyJob* job = KIO::move( u, "trash:/" );
    job->setInteractive( false ); // no skip dialog, thanks
    QMap<QString, QString> metaData;
    //bool ok = KIO::NetAccess::move( u, "trash:/" );
    bool ok = KIO::NetAccess::synchronousRun( job, 0, 0, 0, &metaData );
    assert( !ok );
    assert( KIO::NetAccess::lastError() == KIO::ERR_ACCESS_DENIED );
    const QString infoPath( m_trashDir + "/info/" + fileId + ".trashinfo" );
    assert( !QFile::exists( infoPath ) );

    QFileInfo files( m_trashDir + "/files/" + fileId );
    assert( !files.exists() );

    assert( QFile::exists( u.path() ) );
}

void TestTrash::trashSymlink( const QString& origFilePath, const QString& fileId, bool broken )
{
    kdDebug() << k_funcinfo << endl;
    // setup
    const char* target = broken ? "/nonexistent" : "/tmp";
    bool ok = ::symlink( target, QFile::encodeName( origFilePath ) ) == 0;
    assert( ok );
    KURL u;
    u.setPath( origFilePath );

    // test
    ok = KIO::NetAccess::move( u, "trash:/" );
    assert( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kdDebug() << " TESTS SKIPPED" << endl;
        return;
    }
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
    trashSymlink( homeTmpDir() + fileName, fileName, false );
}

void TestTrash::trashSymlinkFromOther()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "symlinkFromOther";
    trashSymlink( otherTmpDir() + fileName, fileName, false );
}

void TestTrash::trashBrokenSymlinkFromHome()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileName = "brokenSymlinkFromHome";
    trashSymlink( homeTmpDir() + fileName, fileName, true );
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
    if ( origPath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kdDebug() << " TESTS SKIPPED" << endl;
        return;
    }
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
    kdDebug() << k_funcinfo << " with file_move" << endl;
    bool worked = KIO::NetAccess::file_move( "trash:/0-tryRenameInsideTrash", "trash:/foobar" );
    assert( !worked );
    assert( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );

    kdDebug() << k_funcinfo << " with move" << endl;
    worked = KIO::NetAccess::move( "trash:/0-tryRenameInsideTrash", "trash:/foobar" );
    assert( !worked );
    assert( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );
}

void TestTrash::delRootFile()
{
    kdDebug() << k_funcinfo << endl;

    // test deleting a trashed file
    bool ok = KIO::NetAccess::del( "trash:/0-fileFromHome", 0L );
    assert( ok );

    QFileInfo file( m_trashDir + "/files/fileFromHome" );
    assert( !file.exists() );
    QFileInfo info( m_trashDir + "/info/fileFromHome.trashinfo" );
    assert( !info.exists() );

    // trash it again, we might need it later
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::delFileInDirectory()
{
    kdDebug() << k_funcinfo << endl;

    // test deleting a file inside a trashed directory -> not allowed
    bool ok = KIO::NetAccess::del( "trash:/0-trashDirFromHome/testfile", 0L );
    assert( !ok );
    assert( KIO::NetAccess::lastError() == KIO::ERR_ACCESS_DENIED );

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
    bool ok = KIO::NetAccess::del( "trash:/0-trashDirFromHome", 0L );
    assert( ok );

    QFileInfo dir( m_trashDir + "/files/trashDirFromHome" );
    assert( !dir.exists() );
    QFileInfo file( m_trashDir + "/files/trashDirFromHome/testfile" );
    assert( !file.exists() );
    QFileInfo info( m_trashDir + "/info/trashDirFromHome.trashinfo" );
    assert( !info.exists() );

    // trash it again, we'll need it later
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
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
    KURL url( "trash:/0-fileFromHome" );
    KIO::UDSEntry entry;
    bool ok = KIO::NetAccess::stat( url, entry, 0 );
    assert( ok );
    KFileItem item( entry, url );
    assert( item.isFile() );
    assert( !item.isDir() );
    assert( !item.isLink() );
    assert( item.isReadable() );
    assert( !item.isWritable() );
    assert( !item.isHidden() );
    assert( item.name() == "fileFromHome" );
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
    KURL url( "trash:/0-symlinkFromHome" );
    KIO::UDSEntry entry;
    bool ok = KIO::NetAccess::stat( url, entry, 0 );
    assert( ok );
    KFileItem item( entry, url );
    assert( item.isLink() );
    assert( item.linkDest() == "/tmp" );
    assert( item.isReadable() );
    assert( !item.isWritable() );
    assert( !item.isHidden() );
    assert( item.name() == "symlinkFromHome" );
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

    assert( KIO::NetAccess::exists( src, true, (QWidget*)0 ) );

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
    const QString destPath = otherTmpDir() + "fileFromHome_copied";
    copyFromTrash( "fileFromHome", destPath );
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
    const QString destPath = otherTmpDir() + "trashDirFromHome_copied";
    copyFromTrash( "trashDirFromHome", destPath );
    assert( QFileInfo( destPath ).isDir() );
}

void TestTrash::copySymlinkFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "symlinkFromHome_copied";
    copyFromTrash( "symlinkFromHome", destPath );
    assert( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::moveFromTrash( const QString& fileId, const QString& destPath, const QString& relativePath )
{
    KURL src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KURL dest;
    dest.setPath( destPath );

    assert( KIO::NetAccess::exists( src, true, (QWidget*)0 ) );

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
    const QString destPath = otherTmpDir() + "fileFromHome_restored";
    moveFromTrash( "fileFromHome", destPath );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );

    // trash it again for later
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::moveFileInDirectoryFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "testfile_restored";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::moveDirectoryFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "trashDirFromHome_restored";
    moveFromTrash( "trashDirFromHome", destPath );
    assert( QFileInfo( destPath ).isDir() );

    // trash it again, we'll need it later
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
}

void TestTrash::trashDirectoryOwnedByRoot()
{
    KURL u;
    if ( QFile::exists( "/etc/cups" ) )
        u.setPath( "/etc/cups" );
    else if ( QFile::exists( "/boot" ) )
        u.setPath( "/boot" );
    else
        u.setPath( "/etc" );
    const QString fileId = u.path();
    kdDebug() << k_funcinfo << "fileId=" << fileId << endl;

    KIO::CopyJob* job = KIO::move( u, "trash:/" );
    job->setInteractive( false ); // no skip dialog, thanks
    QMap<QString, QString> metaData;
    bool ok = KIO::NetAccess::synchronousRun( job, 0, 0, 0, &metaData );
    assert( !ok );
    const int err = KIO::NetAccess::lastError();
    assert( err == KIO::ERR_ACCESS_DENIED
            || err == KIO::ERR_CANNOT_OPEN_FOR_READING );

    const QString infoPath( m_trashDir + "/info/" + fileId + ".trashinfo" );
    assert( !QFile::exists( infoPath ) );

    QFileInfo files( m_trashDir + "/files/" + fileId );
    assert( !files.exists() );

    assert( QFile::exists( u.path() ) );
}

void TestTrash::moveSymlinkFromTrash()
{
    kdDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "symlinkFromHome_restored";
    moveFromTrash( "symlinkFromHome", destPath );
    assert( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::getFile()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileId = "fileFromHome_1";
    const KURL url = TrashImpl::makeURL( 0, fileId, QString::null );
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
    const KURL url = TrashImpl::makeURL( 0, fileId, QString::null );
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

void TestTrash::restoreFileFromSubDir()
{
    kdDebug() << k_funcinfo << endl;
    const QString fileId = "trashDirFromHome_1/testfile";
    assert( !QFile::exists( homeTmpDir() + "trashDirFromHome_1" ) );

    const KURL url = TrashImpl::makeURL( 0, fileId, QString::null );
    const QString infoFile( m_trashDir + "/info/trashDirFromHome_1.trashinfo" );
    const QString filesItem( m_trashDir + "/files/trashDirFromHome_1/testfile" );

    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( packedArgs, IO_WriteOnly );
    stream << (int)3 << url;
    KIO::Job* job = KIO::special( url, packedArgs );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( !ok );
    // dest dir doesn't exist -> error message
    assert( KIO::NetAccess::lastError() == KIO::ERR_SLAVE_DEFINED );

    // check that nothing happened
    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );
    assert( !QFile::exists( homeTmpDir() + "trashDirFromHome_1" ) );
}

void TestTrash::restoreFileToDeletedDirectory()
{
    kdDebug() << k_funcinfo << endl;
    // Ensure we'll get "fileFromHome" as fileId
    removeFile( m_trashDir, "/info/fileFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromHome" );
    trashFileFromHome();
    // Delete orig dir
    bool delOK = KIO::NetAccess::del( homeTmpDir(), 0 );
    assert( delOK );

    const QString fileId = "fileFromHome";
    const KURL url = TrashImpl::makeURL( 0, fileId, QString::null );
    const QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    const QString filesItem( m_trashDir + "/files/" + fileId );

    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( packedArgs, IO_WriteOnly );
    stream << (int)3 << url;
    KIO::Job* job = KIO::special( url, packedArgs );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( !ok );
    // dest dir doesn't exist -> error message
    assert( KIO::NetAccess::lastError() == KIO::ERR_SLAVE_DEFINED );

    // check that nothing happened
    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    const QString destPath = homeTmpDir() + "fileFromHome";
    assert( !QFile::exists( destPath ) );
}

void TestTrash::listRootDir()
{
    kdDebug() << k_funcinfo << endl;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( "trash:/" );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kdDebug() << "listDir done - m_entryCount=" << m_entryCount << endl;
    assert( m_entryCount > 1 );

    kdDebug() << k_funcinfo << m_listResult << endl;
    assert( m_listResult.contains( "." ) == 1 ); // found it, and only once
}

void TestTrash::listRecursiveRootDir()
{
    kdDebug() << k_funcinfo << endl;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listRecursive( "trash:/" );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kdDebug() << "listDir done - m_entryCount=" << m_entryCount << endl;
    assert( m_entryCount > 1 );

    kdDebug() << k_funcinfo << m_listResult << endl;
    assert( m_listResult.contains( "." ) == 1 ); // found it, and only once
}

void TestTrash::listSubDir()
{
    kdDebug() << k_funcinfo << endl;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( "trash:/0-trashDirFromHome" );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kdDebug() << "listDir done - m_entryCount=" << m_entryCount << endl;
    assert( m_entryCount == 2 );

    kdDebug() << k_funcinfo << m_listResult << endl;
    assert( m_listResult.contains( "." ) == 1 ); // found it, and only once
    assert( m_listResult.contains( "testfile" ) == 1 ); // found it, and only once
}

void TestTrash::slotEntries( KIO::Job*, const KIO::UDSEntryList& lst )
{
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
        kdDebug() << k_funcinfo << displayName << " " << url << endl;
        if ( !url.isEmpty() ) {
            assert( url.protocol() == "trash" );
        }
        m_listResult << displayName;
    }
    m_entryCount += lst.count();
}

void TestTrash::emptyTrash()
{
    // ## Even though we use a custom XDG_DATA_HOME value, emptying the
    // trash would still empty the other trash directories in other partitions.
    // So we can't activate this test by default.
#if 0
    kdDebug() << k_funcinfo << endl;
    QByteArray packedArgs;
    QDataStream stream( packedArgs, IO_WriteOnly );
    stream << (int)1;
    KIO::Job* job = KIO::special( "trash:/", packedArgs );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );

    KSimpleConfig cfg( "trashrc", true );
    assert( cfg.hasGroup( "Status" ) );
    cfg.setGroup( "Status" );
    assert( cfg.readBoolEntry( "Empty", false ) == true );
#else
    kdDebug() << k_funcinfo << " : SKIPPED" << endl;
#endif
}

#include "testtrash.moc"
