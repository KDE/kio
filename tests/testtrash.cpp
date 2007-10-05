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

#include <kurl.h>
#include <klocale.h>
#include <kapplication.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kdebug.h>
#include <kcmdlineargs.h>
#include <kconfiggroup.h>

#include <QDir>
#include <QFileInfo>
#include <QVector>
#include <kjobuidelegate.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <kfileitem.h>
#include <kstandarddirs.h>
#include <kio/chmodjob.h>

static bool check(QString a, QString b)
{
    if (a.isEmpty())
        a.clear();
    if (b.isEmpty())
        b.clear();
    if (a == b) {
        kDebug() << " : checking '" << a << "' against expected value '" << b << "'... " << "ok";
    }
    else {
        kDebug() << " : checking '" << a << "' against expected value '" << b << "'... " << "KO !";
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

    //KApplication::disableAutoDcopRegistration();
    KCmdLineArgs::init(argc,argv,"testtrash", 0, KLocalizedString(), 0);
    KApplication app;

    TestTrash test;
    test.setup();
    test.runAll();
    kDebug() << "All tests OK.";
    return 0; // success. The exit(1) in check() is what happens in case of failure.
}

QString TestTrash::homeTmpDir() const
{
    return QDir::homePath() + "/.kde/testtrash/";
}

QString TestTrash::readOnlyDirPath() const
{
    return homeTmpDir() + QString( "readonly" );
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

static void removeDirRecursive( const QString& dir )
{
    if ( QFileInfo( dir ).exists() ) {

        // Make it work even with readonly dirs, like trashReadOnlyDirFromHome() creates
        KUrl u = KUrl::fromPath( dir );
        KFileItem fileItem( u, "inode/directory", KFileItem::Unknown );
        KFileItemList fileItemList;
        fileItemList.append( fileItem );
        KIO::ChmodJob* chmodJob = KIO::chmod( fileItemList, 0200, 0200, QString(), QString(), true /*recursive*/, KIO::HideProgressInfo );
        KIO::NetAccess::synchronousRun( chmodJob, 0 );

        bool ok = KIO::NetAccess::del( u, 0 );
        if ( !ok )
            kFatal() << "Couldn't delete " << dir ;
    }
}

void TestTrash::setup()
{
    m_trashDir = KGlobal::dirs()->localxdgdatadir() + "Trash";
    kDebug() << "setup: using trash directory " << m_trashDir;

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
                    kDebug() << "/tmp is on its own partition (trashid=" << m_tmpTrashId << "), some tests will be skipped";
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
        kWarning() << "No writable partition other than $HOME found, some tests will be skipped" ;

    // Start with a clean base dir
    removeDirRecursive( homeTmpDir() );
    removeDirRecursive( otherTmpDir() );

    QDir dir; // TT: why not a static method?
    bool ok = dir.mkdir( homeTmpDir() );
    if ( !ok )
        kFatal() << "Couldn't create " << homeTmpDir() ;
    ok = dir.mkdir( otherTmpDir() );
    if ( !ok )
        kFatal() << "Couldn't create " << otherTmpDir() ;

    kDebug() ;
    // Start with a clean trash too
    removeDirRecursive( m_trashDir );
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
    trashReadOnlyDirFromHome();
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
    kDebug() << infoPath;
    QFileInfo info( infoPath );
    assert( info.exists() );
    assert( info.isFile() );
    KConfig infoFile( info.absoluteFilePath() );
    KConfigGroup group = infoFile.group( "Trash Info" );
    if ( !group.exists() )
        kFatal() << "no Trash Info group in " << info.absoluteFilePath() ;
    const QString origPath = group.readEntry( "Path" );
    assert( !origPath.isEmpty() );
    assert( origPath == QUrl::toPercentEncoding( origFilePath.toLatin1() ) );
    const QString date = group.readEntry( "DeletionDate" );
    assert( !date.isEmpty() );
    assert( date.contains( "T" ) );
}

static void createTestFile( const QString& path )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly ) )
        kFatal() << "Can't create " << path ;
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
    KIO::Job* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    QMap<QString, QString> metaData;
    bool ok = KIO::NetAccess::synchronousRun( job, 0, 0, 0, &metaData );
    if ( !ok )
        kError() << "moving " << u << " to trash failed with error " << KIO::NetAccess::lastError() << " " << KIO::NetAccess::lastErrorString() << endl;
    assert( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED";
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
            kDebug() << trashURL;
            assert( !trashURL.isEmpty() );
            assert( trashURL.protocol() == "trash" );
            int trashId = 0;
            if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition )
                trashId = m_tmpTrashId;
            assert( trashURL.path() == "/" + QString::number( trashId ) + '-' + fileId );
            found = true;
        }
    }
    assert( found );
}

void TestTrash::trashFileFromHome()
{
    kDebug() ;
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );

    // Do it again, check that we got a different id
    trashFile( homeTmpDir() + fileName, fileName + "_1" );
}

void TestTrash::trashPercentFileFromHome()
{
    kDebug() ;
    const QString fileName = "file%2f";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashUtf8FileFromHome()
{
    kDebug() ;
    const QString fileName = utf8FileName();
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashUmlautFileFromHome()
{
    kDebug() ;
    const QString fileName = umlautFileName();
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::testTrashNotEmpty()
{
    KConfig cfg( "trashrc", KConfig::SimpleConfig );
    const KConfigGroup group = cfg.group( "Status" );
    assert( group.exists() );
    assert( group.readEntry( "Empty", true ) == false );
}

void TestTrash::trashFileFromOther()
{
    kDebug() ;
    const QString fileName = "fileFromOther";
    trashFile( otherTmpDir() + fileName, fileName );
}

void TestTrash::trashFileIntoOtherPartition()
{
    if ( m_otherPartitionTrashDir.isEmpty() ) {
        kDebug() << " - SKIPPED";
        return;
    }
    kDebug() ;
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
    KIO::Job* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
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
            kDebug() << trashURL;
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
    kDebug() ;
    KUrl u;
    u.setPath( "/etc/passwd" );
    const QString fileId = "passwd";

    KIO::CopyJob* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    job->setUiDelegate(0); // no skip dialog, thanks
    QMap<QString, QString> metaData;
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
    kDebug() ;
    // setup
    const char* target = broken ? "/nonexistent" : "/tmp";
    bool ok = ::symlink( target, QFile::encodeName( origFilePath ) ) == 0;
    assert( ok );
    KUrl u;
    u.setPath( origFilePath );

    // test
    KIO::Job* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    ok = job->exec();
    assert( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED";
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
    kDebug() ;
    const QString fileName = "symlinkFromHome";
    trashSymlink( homeTmpDir() + fileName, fileName, false );
}

void TestTrash::trashSymlinkFromOther()
{
    kDebug() ;
    const QString fileName = "symlinkFromOther";
    trashSymlink( otherTmpDir() + fileName, fileName, false );
}

void TestTrash::trashBrokenSymlinkFromHome()
{
    kDebug() ;
    const QString fileName = "brokenSymlinkFromHome";
    trashSymlink( homeTmpDir() + fileName, fileName, true );
}

void TestTrash::trashDirectory( const QString& origPath, const QString& fileId )
{
    kDebug() << fileId;
    // setup
    if ( !QFileInfo( origPath ).exists() ) {
        QDir dir;
        bool ok = dir.mkdir( origPath );
        Q_ASSERT( ok );
    }
    createTestFile( origPath + "/testfile" );
    KUrl u; u.setPath( origPath );

    // test
    KIO::Job* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    assert( job->exec() );
    if ( origPath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED";
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
    kDebug() ;
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
    // Do it again, check that we got a different id
    trashDirectory( homeTmpDir() + dirName, dirName + "_1" );
}

void TestTrash::trashReadOnlyDirFromHome()
{
    kDebug() ;
    const QString dirName = readOnlyDirPath();
    QDir dir;
    bool ok = dir.mkdir( dirName );
    Q_ASSERT( ok );
    // #130780
    const QString subDirPath = dirName + "/readonly_subdir";
    ok = dir.mkdir( subDirPath );
    Q_ASSERT( ok );
    createTestFile( subDirPath + "/testfile_in_subdir" );
    ::chmod( QFile::encodeName( subDirPath ), 0500 );

    trashDirectory( dirName, "readonly" );
}

void TestTrash::trashDirectoryFromOther()
{
    kDebug() ;
    QString dirName = "trashDirFromOther";
    trashDirectory( otherTmpDir() + dirName, dirName );
}

void TestTrash::tryRenameInsideTrash()
{
    kDebug() << " with file_move";
    KIO::Job* job = KIO::file_move( KUrl("trash:/0-tryRenameInsideTrash"), KUrl("trash:/foobar"), KIO::HideProgressInfo );
    bool worked = KIO::NetAccess::synchronousRun( job, 0 );
    assert( !worked );
    assert( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );

    kDebug() << " with move";
    job = KIO::move( KUrl("trash:/0-tryRenameInsideTrash"), KUrl("trash:/foobar"), KIO::HideProgressInfo );
    worked = KIO::NetAccess::synchronousRun( job, 0 );
    assert( !worked );
    assert( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );
}

void TestTrash::delRootFile()
{
    kDebug() ;

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
    kDebug() ;

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
    kDebug() ;

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
    kDebug() ;
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
}

void TestTrash::statFileInRoot()
{
    kDebug() ;
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
}

void TestTrash::statDirectoryInRoot()
{
    kDebug() ;
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
}

void TestTrash::statSymlinkInRoot()
{
    kDebug() ;
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
}

void TestTrash::statFileInDirectory()
{
    kDebug() ;
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
}

void TestTrash::copyFromTrash( const QString& fileId, const QString& destPath, const QString& relativePath )
{
    KUrl src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KUrl dest;
    dest.setPath( destPath );

    assert( KIO::NetAccess::exists( src, KIO::NetAccess::SourceSide, (QWidget*)0 ) );

    // A dnd would use copy(), but we use copyAs to ensure the final filename
    //kDebug() << "copyAs:" << src << " -> " << dest;
    KIO::Job* job = KIO::copyAs( src, dest, KIO::HideProgressInfo );
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
    kDebug() ;
    const QString destPath = otherTmpDir() + "fileFromHome_copied";
    copyFromTrash( "fileFromHome", destPath );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::copyFileInDirectoryFromTrash()
{
    kDebug() ;
    const QString destPath = otherTmpDir() + "testfile_copied";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::copyDirectoryFromTrash()
{
    kDebug() ;
    const QString destPath = otherTmpDir() + "trashDirFromHome_copied";
    copyFromTrash( "trashDirFromHome", destPath );
    assert( QFileInfo( destPath ).isDir() );
}

void TestTrash::copySymlinkFromTrash()
{
    kDebug() ;
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

    assert( KIO::NetAccess::exists( src, KIO::NetAccess::SourceSide, (QWidget*)0 ) );

    // A dnd would use move(), but we use moveAs to ensure the final filename
    KIO::Job* job = KIO::moveAs( src, dest, KIO::HideProgressInfo );
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
    kDebug() ;
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
    kDebug() ;
    const QString destPath = otherTmpDir() + "testfile_restored";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    assert( QFileInfo( destPath ).isFile() );
    assert( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::moveDirectoryFromTrash()
{
    kDebug() ;
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
    kDebug() << "fileId=" << fileId;

    KIO::CopyJob* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    job->setUiDelegate(0); // no skip dialog, thanks
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
    kDebug() ;
    const QString destPath = otherTmpDir() + "symlinkFromHome_restored";
    moveFromTrash( "symlinkFromHome", destPath );
    assert( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::getFile()
{
    kDebug() ;
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
        kFatal() << "get() returned the following data:" << str ;
    file.close();
    KIO::NetAccess::removeTempFile( tmpFile );
}

void TestTrash::restoreFile()
{
    kDebug() ;
    const QString fileId = "fileFromHome_1";
    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    const QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    const QString filesItem( m_trashDir + "/files/" + fileId );

    assert( QFile::exists( infoFile ) );
    assert( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
    stream << (int)3 << url;
    KIO::Job* job = KIO::special( url, packedArgs, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );

    assert( !QFile::exists( infoFile ) );
    assert( !QFile::exists( filesItem ) );

    const QString destPath = homeTmpDir() + "fileFromHome";
    assert( QFile::exists( destPath ) );
}

void TestTrash::restoreFileFromSubDir()
{
    kDebug() ;
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
    KIO::Job* job = KIO::special( url, packedArgs, KIO::HideProgressInfo );
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
    kDebug() ;
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
    KIO::Job* job = KIO::special( url, packedArgs, KIO::HideProgressInfo );
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
    kDebug() ;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( KUrl("trash:/") );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount;
    assert( m_entryCount > 1 );

    kDebug() << m_listResult;
    assert( m_listResult.contains( "." ) == 1 ); // found it, and only once
}

void TestTrash::listRecursiveRootDir()
{
    kDebug() ;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listRecursive( KUrl("trash:/") );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount;
    assert( m_entryCount > 1 );

    kDebug() << m_listResult;
    assert( m_listResult.count( "." ) == 1 ); // found it, and only once
}

void TestTrash::listSubDir()
{
    kDebug() ;
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( KUrl("trash:/0-trashDirFromHome") );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount;
    assert( m_entryCount == 2 );

    kDebug() << m_listResult;
    assert( m_listResult.count( "." ) == 1 ); // found it, and only once
    assert( m_listResult.count( "testfile" ) == 1 ); // found it, and only once
}

void TestTrash::slotEntries( KIO::Job*, const KIO::UDSEntryList& lst )
{
    for( KIO::UDSEntryList::ConstIterator it = lst.begin(); it != lst.end(); ++it ) {
        const KIO::UDSEntry& entry (*it);
        QString displayName = entry.stringValue( KIO::UDSEntry::UDS_NAME );
        KUrl url = entry.stringValue( KIO::UDSEntry::UDS_URL );
        kDebug() << displayName << " " << url;
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
    kDebug() ;
    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
    stream << (int)1;
    KIO::Job* job = KIO::special( KUrl( "trash:/" ), packedArgs, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    assert( ok );

    KConfig cfg( "trashrc", KConfig::SimpleConfig );
    assert( cfg.hasGroup( "Status" ) );
    assert( cfg.group("Status").readEntry( "Empty", false ) == true );

    assert( !QFile::exists( m_trashDir + "/files/fileFromHome" ) );
    assert( !QFile::exists( m_trashDir + "/files/readonly" ) );
    assert( !QFile::exists( m_trashDir + "/info/readonly.trashinfo" ) );
#else
    kDebug() << " : SKIPPED";
#endif
}

static void checkIcon( const KUrl& url, const QString& expectedIcon )
{
    QString icon = KMimeType::iconNameForUrl( url );
    COMPARE( icon, expectedIcon );
}

void TestTrash::testIcons()
{
    checkIcon( KUrl("trash:/"), "user-trash-full" ); // #100321
    checkIcon( KUrl("trash:/foo/"), "inode-directory" );
}

#include "testtrash.moc"
