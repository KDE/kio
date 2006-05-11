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

#include <QDir>
#include <qfileinfo.h>
#include <qvector.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <kfileitem.h>
#include <kstandarddirs.h>

static bool check(QString a, QString b)
{
    if (a.isEmpty())
        a.clear();
    if (b.isEmpty())
        b.clear();
    if (a == b) {
        kDebug() << " : checking '" << a << "' against expected value '" << b << "'... " << "ok" << endl;
    }
    else {
        kDebug() << " : checking '" << a << "' against expected value '" << b << "'... " << "KO !" << endl;
        exit(1);
    }
    return true;
}
// for porting to qttestlib
#define COMPARE check

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
    setenv( "XDG_DATA_HOME", QFile::encodeName( QDir::homePath() + "/.local-testtrash" ), true );
    setenv( "KDE_FORK_SLAVES", "yes", true );

    KApplication::disableAutoDcopRegistration();
    KCmdLineArgs::init(argc,argv,"testtrash", 0, 0, 0);
    KApplication app;

    TestTrash test;
    test.setup();
    test.runAll();
    kDebug() << "All tests OK." << endl;
    return 0; // success. The exit(1) in check() is what happens in case of failure.
}

QString TestTrash::homeTmpDir() const
{
    return QDir::homePath() + "/.kde/testtrash/";
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
    kDebug() << "setup: using trash directory " << m_trashDir << endl;

    // Look for another writable partition than $HOME (not mandatory)
    TrashImpl impl;
    impl.init();

    TrashImpl::TrashDirMap trashDirs = impl.trashDirectories();
    TrashImpl::TrashDirMap topDirs = impl.topDirectories();
    bool foundTrashDir = false;
    m_otherPartitionId = 0;
    m_tmpIsWritablePartition = false;
    m_tmpTrashId = -1;
    QVector<int> writableTopDirs;
    for ( TrashImpl::TrashDirMap::ConstIterator it = trashDirs.begin(); it != trashDirs.end() ; ++it ) {
        if ( it.key() == 0 ) {
            assert( it.value() == m_trashDir );
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
                    kDebug() << "/tmp is on its own partition (trashid=" << m_tmpTrashId << "), some tests will be skipped" << endl;
                    removeFile( it.value(), "/info/fileFromOther.trashinfo" );
                    removeFile( it.value(), "/files/fileFromOther" );
                    removeFile( it.value(), "/info/symlinkFromOther.trashinfo" );
                    removeFile( it.value(), "/files/symlinkFromOther" );
                    removeFile( it.value(), "/info/trashDirFromOther.trashinfo" );
                    removeFile( it.value(), "/files/trashDirFromOther/testfile" );
                    removeDir( it.value(), "/files/trashDirFromOther" );
                }
            }
        }
    }
    for ( QVector<int>::const_iterator it = writableTopDirs.begin(); it != writableTopDirs.end(); ++it ) {
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
            kDebug() << "OK, found another writable partition: topDir=" << m_otherPartitionTopDir
                      << " trashDir=" << m_otherPartitionTrashDir << " id=" << m_otherPartitionId << endl;
            break;
        }
    }
    // Check that m_trashDir got listed
    assert( foundTrashDir );
    if ( m_otherPartitionTrashDir.isEmpty() )
        kWarning() << "No writable partition other than $HOME found, some tests will be skipped" << endl;

    // Start with a clean base dir
    KIO::NetAccess::del( KUrl::fromPath( homeTmpDir() ), 0 );
    KIO::NetAccess::del( KUrl::fromPath( otherTmpDir() ), 0 );
    QDir dir; // TT: why not a static method?
    bool ok = dir.mkdir( homeTmpDir() );
    if ( !ok )
        kFatal() << "Couldn't create " << homeTmpDir() << endl;
    ok = dir.mkdir( otherTmpDir() );
    if ( !ok )
        kFatal() << "Couldn't create " << otherTmpDir() << endl;
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
    KIO::NetAccess::del( KUrl::fromPath( m_trashDir + "/files/cups" ), 0 );
    KIO::NetAccess::del( KUrl::fromPath( m_trashDir + "/files/boot" ), 0 );
    KIO::NetAccess::del( KUrl::fromPath( m_trashDir + "/files/etc" ), 0 );

    //system( "find ~/.local-testtrash/share/Trash" );
}

void TestTrash::runAll()
{
    testIcons();

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
    const KUrl url = TrashImpl::makeURL( 1, "fileId", QString() );
    COMPARE( url.url(), QLatin1String( "trash:/1-fileId" ) );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    assert( ok );
    COMPARE( QString::number( trashId ), QString::fromLatin1( "1" ) );
    COMPARE( fileId, QString::fromLatin1( "fileId" ) );
    COMPARE( relativePath, QString() );
}

void TestTrash::urlTestDirectory()
{
    const KUrl url = TrashImpl::makeURL( 1, "fileId", "subfile" );
    COMPARE( url.url(), QString::fromLatin1( "trash:/1-fileId/subfile" ) );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    assert( ok );
    COMPARE( QString::number( trashId ), QString::fromLatin1( "1" ) );
    COMPARE( fileId, QString::fromLatin1( "fileId" ) );
    COMPARE( relativePath, QString::fromLatin1( "subfile" ) );
}

void TestTrash::urlTestSubDirectory()
{
    const KUrl url = TrashImpl::makeURL( 1, "fileId", "subfile/foobar" );
    COMPARE( url.url(), QString::fromLatin1( "trash:/1-fileId/subfile/foobar" ) );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    assert( ok );
    COMPARE( QString::number( trashId ), QString::fromLatin1( "1" ) );
    COMPARE( fileId, QString::fromLatin1( "fileId" ) );
    COMPARE( relativePath, QString::fromLatin1( "subfile/foobar" ) );
}

static void checkInfoFile( const QString& infoPath, const QString& origFilePath )
{
    kDebug() << k_funcinfo << infoPath << endl;
    QFileInfo info( infoPath );
    assert( info.exists() );
    assert( info.isFile() );
    KSimpleConfig infoFile( info.absoluteFilePath(), true );
    if ( !infoFile.hasGroup( "Trash Info" ) )
        kFatal() << "no Trash Info group in " << info.absoluteFilePath() << endl;
    infoFile.setGroup( "Trash Info" );
    const QString origPath = infoFile.readEntry( "Path" );
    assert( !origPath.isEmpty() );
    assert( origPath == QUrl::toPercentEncoding( origFilePath.toLatin1() ) );
    const QString date = infoFile.readEntry( "DeletionDate" );
    assert( !date.isEmpty() );
    assert( date.contains( "T" ) );
}

static void createTestFile( const QString& path )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly ) )
        kFatal() << "Can't create " << path << endl;
    f.write( "Hello world\n", 12 );
    f.close();
    assert( QFile::exists( path ) );
}

void TestTrash::trashFile( const QString& origFilePath, const QString& fileId )
{
    // setup
    if ( !QFile::exists( origFilePath ) )
        createTestFile( origFilePath );
    KUrl u;
    u.setPath( origFilePath );

    // test
    KIO::Job* job = KIO::move( u, KUrl("trash:/") );
    QMap<QString, QString> metaData;
    //bool ok = KIO::NetAccess::move( u, "trash:/" );
    bool ok = KIO::NetAccess::synchronousRun( job, 0, 0, 0, &metaData );
    if ( !ok )
        kError() << "moving " << u << " to trash failed with error " << KIO::NetAccess::lastError() << " " << KIO::NetAccess::lastErrorString() << endl;
    assert( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED" << endl;
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
            KUrl trashURL( it.value() );
            kDebug() << trashURL << endl;
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
    kDebug() << k_funcinfo << endl;
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );

    // Do it again, check that we got a different id
    trashFile( homeTmpDir() + fileName, fileName + "_1" );
}

void TestTrash::trashPercentFileFromHome()
{
    kDebug() << k_funcinfo << endl;
    const QString fileName = "file%2f";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashUtf8FileFromHome()
{
    kDebug() << k_funcinfo << endl;
    const QString fileName = utf8FileName();
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashUmlautFileFromHome()
{
    kDebug() << k_funcinfo << endl;
    const QString fileName = umlautFileName();
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::testTrashNotEmpty()
{
    KSimpleConfig cfg( "trashrc", true );
    assert( cfg.hasGroup( "Status" ) );
    cfg.setGroup( "Status" );
    assert( cfg.readEntry( "Empty", QVariant(true )).toBool() == false );
}

void TestTrash::trashFileFromOther()
{
    kDebug() << k_funcinfo << endl;
    const QString fileName = "fileFromOther";
    trashFile( otherTmpDir() + fileName, fileName );
}

void TestTrash::trashFileIntoOtherPartition()
{
    if ( m_otherPartitionTrashDir.isEmpty() ) {
        kDebug() << k_funcinfo << " - SKIPPED" << endl;
        return;
    }
    kDebug() << k_funcinfo << endl;
    const QString fileName = "testtrash-file";
    const QString origFilePath = m_otherPartitionTopDir + fileName;
    const QString fileId = fileName;
    // cleanup
    QFile::remove( m_otherPartitionTrashDir + "/info/" + fileId + ".trashinfo" );
    QFile::remove( m_otherPartitionTrashDir + "/files/" + fileId );

    // setup
    if ( !QFile::exists( origFilePath ) )
        createTestFile( origFilePath );
    KUrl u;
    u.setPath( origFilePath );

    // test
    KIO::Job* job = KIO::move( u, KUrl("trash:/") );
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
            KUrl trashURL( it.value() );
            kDebug() << trashURL << endl;
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
    kDebug() << k_funcinfo << endl;
    KUrl u;
    u.setPath( "/etc/passwd" );
    const QString fileId = "passwd";

    KIO::CopyJob* job = KIO::move( u, KUrl("trash:/") );
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
    kDebug() << k_funcinfo << endl;
    // setup
    const char* target = broken ? "/nonexistent" : "/tmp";
    bool ok = ::symlink( target, QFile::encodeName( origFilePath ) ) == 0;
    assert( ok );
    KUrl u;
    u.setPath( origFilePath );

    // test
    ok = KIO::NetAccess::move( u, KUrl("trash:/") );
    assert( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED" << endl;
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
    kDebug() << k_funcinfo << endl;
    const QString fileName = "symlinkFromHome";
    trashSymlink( homeTmpDir() + fileName, fileName, false );
}

void TestTrash::trashSymlinkFromOther()
{
    kDebug() << k_funcinfo << endl;
    const QString fileName = "symlinkFromOther";
    trashSymlink( otherTmpDir() + fileName, fileName, false );
}

void TestTrash::trashBrokenSymlinkFromHome()
{
    kDebug() << k_funcinfo << endl;
    const QString fileName = "brokenSymlinkFromHome";
    trashSymlink( homeTmpDir() + fileName, fileName, true );
}

void TestTrash::trashDirectory( const QString& origPath, const QString& fileId )
{
    kDebug() << k_funcinfo << fileId << endl;
    // setup
    QDir dir;
    bool ok = dir.mkdir( origPath );
    Q_ASSERT( ok );
    createTestFile( origPath + "/testfile" );
    KUrl u; u.setPath( origPath );

    // test
    ok = KIO::NetAccess::move( u, KUrl("trash:/") );
    assert( ok );
    if ( origPath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED" << endl;
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
    kDebug() << k_funcinfo << endl;
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
    // Do it again, check that we got a different id
    trashDirectory( homeTmpDir() + dirName, dirName + "_1" );
}

void TestTrash::trashDirectoryFromOther()
{
    kDebug() << k_funcinfo << endl;
    QString dirName = "trashDirFromOther";
    trashDirectory( otherTmpDir() + dirName, dirName );
}

void TestTrash::tryRenameInsideTrash()
{
    kDebug() << k_funcinfo << " with file_move" << endl;
    bool worked = KIO::NetAccess::file_move( KUrl("trash:/0-tryRenameInsideTrash"), KUrl("trash:/foobar") );
    assert( !worked );
    assert( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );

    kDebug() << k_funcinfo << " with move" << endl;
    worked = KIO::NetAccess::move( KUrl("trash:/0-tryRenameInsideTrash"), KUrl("trash:/foobar") );
    assert( !worked );
    assert( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );
}

void TestTrash::delRootFile()
{
    kDebug() << k_funcinfo << endl;

    // test deleting a trashed file
    bool ok = KIO::NetAccess::del( KUrl("trash:/0-fileFromHome"), 0L );
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
    kDebug() << k_funcinfo << endl;

    // test deleting a file inside a trashed directory -> not allowed
    bool ok = KIO::NetAccess::del( KUrl("trash:/0-trashDirFromHome/testfile"), 0L );
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
    kDebug() << k_funcinfo << endl;

    // test deleting a trashed directory
    bool ok = KIO::NetAccess::del( KUrl("trash:/0-trashDirFromHome"), 0L );
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
    kDebug() << k_funcinfo << endl;
    KUrl url( "trash:/" );
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
    kDebug() << k_funcinfo << endl;
    KUrl url( "trash:/0-fileFromHome" );
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
    kDebug() << k_funcinfo << endl;
    KUrl url( "trash:/0-trashDirFromHome" );
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
    kDebug() << k_funcinfo << endl;
    KUrl url( "trash:/0-symlinkFromHome" );
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
    kDebug() << k_funcinfo << endl;
    KUrl url( "trash:/0-trashDirFromHome/testfile" );
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
    KUrl src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KUrl dest;
    dest.setPath( destPath );

    assert( KIO::NetAccess::exists( src, true, (QWidget*)0 ) );

    // A dnd would use copy(), but we use copyAs to ensure the final filename
    //kDebug() << k_funcinfo << "copyAs:" << src << " -> " << dest << endl;
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
    kDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "fileFromHome_copied";
    copyFromTrash( "fileFromHome", destPath );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::copyFileInDirectoryFromTrash()
{
    kDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "testfile_copied";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::copyDirectoryFromTrash()
{
    kDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "trashDirFromHome_copied";
    copyFromTrash( "trashDirFromHome", destPath );
    assert( QFileInfo( destPath ).isDir() );
}

void TestTrash::copySymlinkFromTrash()
{
    kDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "symlinkFromHome_copied";
    copyFromTrash( "symlinkFromHome", destPath );
    assert( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::moveFromTrash( const QString& fileId, const QString& destPath, const QString& relativePath )
{
    KUrl src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KUrl dest;
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
    kDebug() << k_funcinfo << endl;
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
    kDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "testfile_restored";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::moveDirectoryFromTrash()
{
    kDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "trashDirFromHome_restored";
    moveFromTrash( "trashDirFromHome", destPath );
    assert( QFileInfo( destPath ).isDir() );

    // trash it again, we'll need it later
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
}

void TestTrash::trashDirectoryOwnedByRoot()
{
    KUrl u;
    if ( QFile::exists( "/etc/cups" ) )
        u.setPath( "/etc/cups" );
    else if ( QFile::exists( "/boot" ) )
        u.setPath( "/boot" );
    else
        u.setPath( "/etc" );
    const QString fileId = u.path();
    kDebug() << k_funcinfo << "fileId=" << fileId << endl;

    KIO::CopyJob* job = KIO::move( u, KUrl("trash:/") );
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
    kDebug() << k_funcinfo << endl;
    const QString destPath = otherTmpDir() + "symlinkFromHome_restored";
    moveFromTrash( "symlinkFromHome", destPath );
    assert( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::getFile()
{
    kDebug() << k_funcinfo << endl;
    const QString fileId = "fileFromHome_1";
    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    QString tmpFile;
    bool ok = KIO::NetAccess::download( url, tmpFile, 0 );
    assert( ok );
    QFile file( tmpFile );
    ok = file.open( QIODevice::ReadOnly );
    assert( ok );
    QByteArray str = file.readAll();
    if ( str != "Hello world\n" )
        kFatal() << "get() returned the following data:" << str << endl;
    file.close();
    KIO::NetAccess::removeTempFile( tmpFile );
}

void TestTrash::restoreFile()
{
    kDebug() << k_funcinfo << endl;
    const QString fileId = "fileFromHome_1";
    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    const QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    const QString filesItem( m_trashDir + "/files/" + fileId );

    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
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
    kDebug() << k_funcinfo << endl;
    const QString fileId = "trashDirFromHome_1/testfile";
    assert( !QFile::exists( homeTmpDir() + "trashDirFromHome_1" ) );

    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    const QString infoFile( m_trashDir + "/info/trashDirFromHome_1.trashinfo" );
    const QString filesItem( m_trashDir + "/files/trashDirFromHome_1/testfile" );

    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
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
    kDebug() << k_funcinfo << endl;
    // Ensure we'll get "fileFromHome" as fileId
    removeFile( m_trashDir, "/info/fileFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromHome" );
    trashFileFromHome();
    // Delete orig dir
    bool delOK = KIO::NetAccess::del( KUrl::fromPath( homeTmpDir() ), 0 );
    assert( delOK );

    const QString fileId = "fileFromHome";
    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    const QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    const QString filesItem( m_trashDir + "/files/" + fileId );

    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
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
    kDebug() << k_funcinfo << endl;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( KUrl("trash:/") );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount << endl;
    assert( m_entryCount > 1 );

    kDebug() << k_funcinfo << m_listResult << endl;
    assert( m_listResult.contains( "." ) == 1 ); // found it, and only once
}

void TestTrash::listRecursiveRootDir()
{
    kDebug() << k_funcinfo << endl;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listRecursive( KUrl("trash:/") );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount << endl;
    assert( m_entryCount > 1 );

    kDebug() << k_funcinfo << m_listResult << endl;
    assert( m_listResult.contains( "." ) == 1 ); // found it, and only once
}

void TestTrash::listSubDir()
{
    kDebug() << k_funcinfo << endl;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( KUrl("trash:/0-trashDirFromHome") );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount << endl;
    assert( m_entryCount == 2 );

    kDebug() << k_funcinfo << m_listResult << endl;
    assert( m_listResult.contains( "." ) == 1 ); // found it, and only once
    assert( m_listResult.contains( "testfile" ) == 1 ); // found it, and only once
}

void TestTrash::slotEntries( KIO::Job*, const KIO::UDSEntryList& lst )
{
    for( KIO::UDSEntryList::ConstIterator it = lst.begin(); it != lst.end(); ++it ) {
        const KIO::UDSEntry& entry (*it);
        QString displayName = entry.stringValue( KIO::UDS_NAME );
        KUrl url = entry.stringValue( KIO::UDS_URL );
        kDebug() << k_funcinfo << displayName << " " << url << endl;
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
    kDebug() << k_funcinfo << endl;
    QByteArray packedArgs;
    QDataStream stream( packedArgs, QIODevice::WriteOnly );
    stream << (int)1;
    KIO::Job* job = KIO::special( "trash:/", packedArgs );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );

    KSimpleConfig cfg( "trashrc", true );
    assert( cfg.hasGroup( "Status" ) );
    cfg.setGroup( "Status" );
    assert( cfg.readEntry( "Empty", QVariant(false )).toBool() == true );
#else
    kDebug() << k_funcinfo << " : SKIPPED" << endl;
#endif
}

static void checkIcon( const KUrl& url, const QString& expectedIcon )
{
    QString icon = KMimeType::iconNameForURL( url );
    COMPARE( icon, expectedIcon );
}

void TestTrash::testIcons()
{
    checkIcon( KUrl("trash:/"), "trashcan_full" ); // #100321
    checkIcon( KUrl("trash:/foo/"), "folder" );
}

#include "testtrash.moc"
