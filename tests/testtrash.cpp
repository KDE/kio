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

#include <qtest_kde.h>

#include "kio_trash.h"
#include "testtrash.h"
#include <ktemporaryfile.h>

#include <kurl.h>
#include <klocale.h>
#include <kapplication.h>
#include <kio/netaccess.h>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
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
#include <kfileitem.h>
#include <kstandarddirs.h>
#include <kio/chmodjob.h>

// There are two ways to test encoding things:
// * with utf8 filenames
// * with latin1 filenames -- not sure this still works.
//
#define UTF8TEST 1


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
    QVERIFY( !QDir( trashDir + fileName ).exists() );
}

static void removeDir( const QString& trashDir, const QString& dirName )
{
    QDir dir;
    dir.rmdir( trashDir + dirName );
    QVERIFY( !QDir( trashDir + dirName ).exists() );
}

static void removeDirRecursive( const QString& dir )
{
    if ( QFile::exists( dir ) ) {

        // Make it work even with readonly dirs, like trashReadOnlyDirFromHome() creates
        KUrl u = KUrl::fromPath( dir );
        KFileItem fileItem( u, "inode/directory", KFileItem::Unknown );
        KFileItemList fileItemList;
        fileItemList.append( fileItem );
        KIO::ChmodJob* chmodJob = KIO::chmod( fileItemList, 0200, 0200, QString(), QString(), true /*recursive*/, KIO::HideProgressInfo );
        KIO::NetAccess::synchronousRun( chmodJob, 0 );

        KIO::Job* delJob = KIO::del(u, KIO::HideProgressInfo);
        if (!KIO::NetAccess::synchronousRun(delJob, 0))
            kFatal() << "Couldn't delete " << dir ;
    }
}

void TestTrash::initTestCase()
{
#ifdef UTF8TEST
    // Assume utf8 system
    setenv( "LC_ALL", "en_GB.utf-8", 1 );
    setenv( "KDE_UTF8_FILENAMES", "true", 1 );
#else
    // Ensure a known QFile::encodeName behavior for trashUtf8FileFromHome
    // However this assume your $HOME doesn't use characters from other locales...
    setenv( "LC_ALL", "en_GB.ISO-8859-1", 1 );
    unsetenv( "KDE_UTF8_FILENAMES" );
#endif

    setenv( "KDE_FORK_SLAVES", "yes", true );


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
            QVERIFY( it.value() == m_trashDir );
            QVERIFY( topDirs.find( 0 ) == topDirs.end() );
            foundTrashDir = true;
        } else {
            QVERIFY( topDirs.find( it.key() ) != topDirs.end() );
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
        QVERIFY( !topdir.isEmpty() );
        QVERIFY( !trashDirs.isEmpty() );
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
    QVERIFY( foundTrashDir );
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

    // Start with a clean trash too
    removeDirRecursive( m_trashDir );
}

void TestTrash::urlTestFile()
{
    const KUrl url = TrashImpl::makeURL( 1, "fileId", QString() );
    QCOMPARE( url.url(), QString( "trash:/1-fileId" ) );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    QVERIFY( ok );
    QCOMPARE( QString::number( trashId ), QString::fromLatin1( "1" ) );
    QCOMPARE( fileId, QString::fromLatin1( "fileId" ) );
    QCOMPARE( relativePath, QString() );
}

void TestTrash::urlTestDirectory()
{
    const KUrl url = TrashImpl::makeURL( 1, "fileId", "subfile" );
    QCOMPARE( url.url(), QString::fromLatin1( "trash:/1-fileId/subfile" ) );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    QVERIFY( ok );
    QCOMPARE( trashId, 1 );
    QCOMPARE( fileId, QString::fromLatin1( "fileId" ) );
    QCOMPARE( relativePath, QString::fromLatin1( "subfile" ) );
}

void TestTrash::urlTestSubDirectory()
{
    const KUrl url = TrashImpl::makeURL( 1, "fileId", "subfile/foobar" );
    QCOMPARE( url.url(), QString::fromLatin1( "trash:/1-fileId/subfile/foobar" ) );

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL( url, trashId, fileId, relativePath );
    QVERIFY( ok );
    QCOMPARE( trashId, 1 );
    QCOMPARE( fileId, QString::fromLatin1( "fileId" ) );
    QCOMPARE( relativePath, QString::fromLatin1( "subfile/foobar" ) );
}

static void checkInfoFile( const QString& infoPath, const QString& origFilePath )
{
    kDebug() << infoPath;
    QFileInfo info( infoPath );
    QVERIFY( info.exists() );
    QVERIFY( info.isFile() );
    KConfig infoFile( info.absoluteFilePath() );
    KConfigGroup group = infoFile.group( "Trash Info" );
    if ( !group.exists() )
        kFatal() << "no Trash Info group in " << info.absoluteFilePath() ;
    const QString origPath = group.readEntry( "Path" );
    QVERIFY( !origPath.isEmpty() );
    QVERIFY( origPath == QUrl::toPercentEncoding( origFilePath, "/" ) );
    if (origFilePath.contains(QChar(0x2153)) || origFilePath.contains('%') || origFilePath.contains("umlaut")) {
        QVERIFY(origPath.contains('%'));
    } else {
        QVERIFY(!origPath.contains('%'));
    }
    const QString date = group.readEntry( "DeletionDate" );
    QVERIFY( !date.isEmpty() );
    QVERIFY( date.contains( "T" ) );
}

static void createTestFile( const QString& path )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly ) )
        kFatal() << "Can't create " << path ;
    f.write( "Hello world\n", 12 );
    f.close();
    QVERIFY( QFile::exists( path ) );
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
    QVERIFY( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED";
    } else {
        checkInfoFile( m_trashDir + "/info/" + fileId + ".trashinfo", origFilePath );

        QFileInfo files( m_trashDir + "/files/" + fileId );
        QVERIFY( files.isFile() );
        QVERIFY( files.size() == 12 );
    }

    // coolo suggests testing that the original file is actually gone, too :)
    QVERIFY( !QFile::exists( origFilePath ) );

    QVERIFY( !metaData.isEmpty() );
    bool found = false;
    QMap<QString, QString>::ConstIterator it = metaData.begin();
    for ( ; it != metaData.end() ; ++it ) {
        if ( it.key().startsWith( "trashURL" ) ) {
            const QString origPath = it.key().mid( 9 );
            KUrl trashURL( it.value() );
            kDebug() << trashURL;
            QVERIFY( !trashURL.isEmpty() );
            QVERIFY( trashURL.protocol() == "trash" );
            int trashId = 0;
            if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition )
                trashId = m_tmpTrashId;
            QVERIFY( trashURL.path() == "/" + QString::number( trashId ) + '-' + fileId );
            found = true;
        }
    }
    QVERIFY( found );
}

void TestTrash::trashFileFromHome()
{
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );

    // Do it again, check that we got a different id
    trashFile( homeTmpDir() + fileName, fileName + "_1" );
}

void TestTrash::trashPercentFileFromHome()
{
    const QString fileName = "file%2f";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::trashUtf8FileFromHome()
{
#ifdef UTF8TEST
    const QString fileName = utf8FileName();
    trashFile( homeTmpDir() + fileName, fileName );
#endif
}

void TestTrash::trashUmlautFileFromHome()
{
    const QString fileName = umlautFileName();
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::testTrashNotEmpty()
{
    KConfig cfg( "trashrc", KConfig::SimpleConfig );
    const KConfigGroup group = cfg.group( "Status" );
    QVERIFY( group.exists() );
    QVERIFY( group.readEntry( "Empty", true ) == false );
}

void TestTrash::trashFileFromOther()
{
    const QString fileName = "fileFromOther";
    trashFile( otherTmpDir() + fileName, fileName );
}

void TestTrash::trashFileIntoOtherPartition()
{
    if ( m_otherPartitionTrashDir.isEmpty() ) {
        kDebug() << " - SKIPPED";
        return;
    }
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
    QVERIFY( ok );
    // Note that the Path stored in the info file is relative, on other partitions (#95652)
    checkInfoFile( m_otherPartitionTrashDir + "/info/" + fileId + ".trashinfo", fileName );

    QFileInfo files( m_otherPartitionTrashDir + "/files/" + fileId );
    QVERIFY( files.isFile() );
    QVERIFY( files.size() == 12 );

    // coolo suggests testing that the original file is actually gone, too :)
    QVERIFY( !QFile::exists( origFilePath ) );

    QVERIFY( !metaData.isEmpty() );
    bool found = false;
    QMap<QString, QString>::ConstIterator it = metaData.begin();
    for ( ; it != metaData.end() ; ++it ) {
        if ( it.key().startsWith( "trashURL" ) ) {
            const QString origPath = it.key().mid( 9 );
            KUrl trashURL( it.value() );
            kDebug() << trashURL;
            QVERIFY( !trashURL.isEmpty() );
            QVERIFY( trashURL.protocol() == "trash" );
            QVERIFY( trashURL.path() == QString( "/%1-%2" ).arg( m_otherPartitionId ).arg( fileId ) );
            found = true;
        }
    }
    QVERIFY( found );
}

void TestTrash::trashFileOwnedByRoot()
{
    KUrl u( "/etc/passwd" );
    const QString fileId = "passwd";

    KIO::CopyJob* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    job->setUiDelegate(0); // no skip dialog, thanks
    QMap<QString, QString> metaData;
    bool ok = KIO::NetAccess::synchronousRun( job, 0, 0, 0, &metaData );
    QVERIFY( !ok );
    QVERIFY( KIO::NetAccess::lastError() == KIO::ERR_ACCESS_DENIED );
    const QString infoPath( m_trashDir + "/info/" + fileId + ".trashinfo" );
    QVERIFY( !QFile::exists( infoPath ) );

    QFileInfo files( m_trashDir + "/files/" + fileId );
    QVERIFY( !files.exists() );

    QVERIFY( QFile::exists( u.path() ) );
}

void TestTrash::trashSymlink( const QString& origFilePath, const QString& fileId, bool broken )
{
    // setup
    const char* target = broken ? "/nonexistent" : "/tmp";
    bool ok = ::symlink( target, QFile::encodeName( origFilePath ) ) == 0;
    QVERIFY( ok );
    KUrl u;
    u.setPath( origFilePath );

    // test
    KIO::Job* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    ok = job->exec();
    QVERIFY( ok );
    if ( origFilePath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED";
        return;
    }
    checkInfoFile( m_trashDir + "/info/" + fileId + ".trashinfo", origFilePath );

    QFileInfo files( m_trashDir + "/files/" + fileId );
    QVERIFY( files.isSymLink() );
    QVERIFY( files.readLink() == QFile::decodeName( target ) );
    QVERIFY( !QFile::exists( origFilePath ) );
}

void TestTrash::trashSymlinkFromHome()
{
    const QString fileName = "symlinkFromHome";
    trashSymlink( homeTmpDir() + fileName, fileName, false );
}

void TestTrash::trashSymlinkFromOther()
{
    const QString fileName = "symlinkFromOther";
    trashSymlink( otherTmpDir() + fileName, fileName, false );
}

void TestTrash::trashBrokenSymlinkFromHome()
{
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
        QVERIFY( ok );
    }
    createTestFile( origPath + "/testfile" );
    KUrl u; u.setPath( origPath );

    // test
    KIO::Job* job = KIO::move( u, KUrl("trash:/"), KIO::HideProgressInfo );
    QVERIFY( job->exec() );
    if ( origPath.startsWith( "/tmp" ) && m_tmpIsWritablePartition ) {
        kDebug() << " TESTS SKIPPED";
        return;
    }
    checkInfoFile( m_trashDir + "/info/" + fileId + ".trashinfo", origPath );

    QFileInfo filesDir( m_trashDir + "/files/" + fileId );
    QVERIFY( filesDir.isDir() );
    QFileInfo files( m_trashDir + "/files/" + fileId + "/testfile" );
    QVERIFY( files.exists() );
    QVERIFY( files.isFile() );
    QVERIFY( files.size() == 12 );
    QVERIFY( !QFile::exists( origPath ) );
}

void TestTrash::trashDirectoryFromHome()
{
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
    // Do it again, check that we got a different id
    trashDirectory( homeTmpDir() + dirName, dirName + "_1" );
}

void TestTrash::trashReadOnlyDirFromHome()
{
    const QString dirName = readOnlyDirPath();
    QDir dir;
    bool ok = dir.mkdir( dirName );
    QVERIFY( ok );
    // #130780
    const QString subDirPath = dirName + "/readonly_subdir";
    ok = dir.mkdir( subDirPath );
    QVERIFY( ok );
    createTestFile( subDirPath + "/testfile_in_subdir" );
    ::chmod( QFile::encodeName( subDirPath ), 0500 );

    trashDirectory( dirName, "readonly" );
}

void TestTrash::trashDirectoryFromOther()
{
    QString dirName = "trashDirFromOther";
    trashDirectory( otherTmpDir() + dirName, dirName );
}

void TestTrash::tryRenameInsideTrash()
{
    kDebug() << " with file_move";
    KIO::Job* job = KIO::file_move( KUrl("trash:/0-tryRenameInsideTrash"), KUrl("trash:/foobar"), -1, KIO::HideProgressInfo );
    bool worked = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( !worked );
    QVERIFY( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );

    kDebug() << " with move";
    job = KIO::move( KUrl("trash:/0-tryRenameInsideTrash"), KUrl("trash:/foobar"), KIO::HideProgressInfo );
    worked = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( !worked );
    QVERIFY( KIO::NetAccess::lastError() == KIO::ERR_CANNOT_RENAME );
}

void TestTrash::delRootFile()
{
    // test deleting a trashed file
    KIO::Job* delJob = KIO::del(KUrl("trash:/0-fileFromHome"), KIO::HideProgressInfo);
    bool ok = KIO::NetAccess::synchronousRun(delJob, 0);
    QVERIFY( ok );

    QFileInfo file( m_trashDir + "/files/fileFromHome" );
    QVERIFY( !file.exists() );
    QFileInfo info( m_trashDir + "/info/fileFromHome.trashinfo" );
    QVERIFY( !info.exists() );

    // trash it again, we might need it later
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::delFileInDirectory()
{
    // test deleting a file inside a trashed directory -> not allowed
    KIO::Job* delJob = KIO::del(KUrl("trash:/0-trashDirFromHome/testfile"), KIO::HideProgressInfo);
    bool ok = KIO::NetAccess::synchronousRun(delJob, 0);
    QVERIFY( !ok );
    QVERIFY( KIO::NetAccess::lastError() == KIO::ERR_ACCESS_DENIED );

    QFileInfo dir( m_trashDir + "/files/trashDirFromHome" );
    QVERIFY( dir.exists() );
    QFileInfo file( m_trashDir + "/files/trashDirFromHome/testfile" );
    QVERIFY( file.exists() );
    QFileInfo info( m_trashDir + "/info/trashDirFromHome.trashinfo" );
    QVERIFY( info.exists() );
}

void TestTrash::delDirectory()
{
    // test deleting a trashed directory
    KIO::Job* delJob = KIO::del(KUrl("trash:/0-trashDirFromHome"), KIO::HideProgressInfo);
    bool ok = KIO::NetAccess::synchronousRun(delJob, 0);
    QVERIFY( ok );

    QFileInfo dir( m_trashDir + "/files/trashDirFromHome" );
    QVERIFY( !dir.exists() );
    QFileInfo file( m_trashDir + "/files/trashDirFromHome/testfile" );
    QVERIFY( !file.exists() );
    QFileInfo info( m_trashDir + "/info/trashDirFromHome.trashinfo" );
    QVERIFY( !info.exists() );

    // trash it again, we'll need it later
    QString dirName = "trashDirFromHome";
    trashDirectory( homeTmpDir() + dirName, dirName );
}

// KIO::NetAccess::stat() doesn't set HideProgressInfo - but it's not much work to do it ourselves:
static bool MyNetAccess_stat(const KUrl& url, KIO::UDSEntry& entry)
{
    KIO::StatJob * statJob = KIO::stat( url, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun(statJob, 0);
    if (ok)
        entry = statJob->statResult();
    return ok;
}
static bool MyNetAccess_exists(const KUrl& url)
{
    KIO::UDSEntry dummy;
    return MyNetAccess_stat(url, dummy);
}

void TestTrash::statRoot()
{
    KUrl url( "trash:/" );
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat( url, entry );
    QVERIFY( ok );
    KFileItem item( entry, url );
    QVERIFY( item.isDir() );
    QVERIFY( !item.isLink() );
    QVERIFY( item.isReadable() );
    QVERIFY( item.isWritable() );
    QVERIFY( !item.isHidden() );
    QVERIFY( item.name() == "." );
}

void TestTrash::statFileInRoot()
{
    KUrl url( "trash:/0-fileFromHome" );
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat( url, entry );
    QVERIFY( ok );
    KFileItem item( entry, url );
    QVERIFY( item.isFile() );
    QVERIFY( !item.isDir() );
    QVERIFY( !item.isLink() );
    QVERIFY( item.isReadable() );
    QVERIFY( !item.isWritable() );
    QVERIFY( !item.isHidden() );
    QVERIFY( item.text() == "fileFromHome" );
}

void TestTrash::statDirectoryInRoot()
{
    KUrl url( "trash:/0-trashDirFromHome" );
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat( url, entry );
    QVERIFY( ok );
    KFileItem item( entry, url );
    QVERIFY( item.isDir() );
    QVERIFY( !item.isLink() );
    QVERIFY( item.isReadable() );
    QVERIFY( !item.isWritable() );
    QVERIFY( !item.isHidden() );
    QVERIFY( item.text() == "trashDirFromHome" );
}

void TestTrash::statSymlinkInRoot()
{
    KUrl url( "trash:/0-symlinkFromHome" );
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat( url, entry );
    QVERIFY( ok );
    KFileItem item( entry, url );
    QVERIFY( item.isLink() );
    QVERIFY( item.linkDest() == "/tmp" );
    QVERIFY( item.isReadable() );
    QVERIFY( !item.isWritable() );
    QVERIFY( !item.isHidden() );
    QVERIFY( item.text() == "symlinkFromHome" );
}

void TestTrash::statFileInDirectory()
{
    KUrl url( "trash:/0-trashDirFromHome/testfile" );
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat( url, entry );
    QVERIFY( ok );
    KFileItem item( entry, url );
    QVERIFY( item.isFile() );
    QVERIFY( !item.isLink() );
    QVERIFY( item.isReadable() );
    QVERIFY( !item.isWritable() );
    QVERIFY( !item.isHidden() );
    QVERIFY( item.text() == "testfile" );
}

void TestTrash::copyFromTrash( const QString& fileId, const QString& destPath, const QString& relativePath )
{
    KUrl src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KUrl dest;
    dest.setPath( destPath );

    QVERIFY(MyNetAccess_exists(src));

    // A dnd would use copy(), but we use copyAs to ensure the final filename
    //kDebug() << "copyAs:" << src << " -> " << dest;
    KIO::Job* job = KIO::copyAs( src, dest, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( ok );
    QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    QVERIFY( QFile::exists(infoFile) );

    QFileInfo filesItem( m_trashDir + "/files/" + fileId );
    QVERIFY( filesItem.exists() );

    QVERIFY( QFile::exists(destPath) );
}

void TestTrash::copyFileFromTrash()
{
// To test case of already-existing destination, uncomment this.
// This brings up the "rename" dialog though, so it can't be fully automated
#if 0
    const QString destPath = otherTmpDir() + "fileFromHome_copied";
    copyFromTrash( "fileFromHome", destPath );
    QVERIFY( QFileInfo( destPath ).isFile() );
    QVERIFY( QFileInfo( destPath ).size() == 12 );
#endif
}

void TestTrash::copyFileInDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + "testfile_copied";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    QVERIFY( QFileInfo( destPath ).isFile() );
    QVERIFY( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::copyDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + "trashDirFromHome_copied";
    copyFromTrash( "trashDirFromHome", destPath );
    QVERIFY( QFileInfo( destPath ).isDir() );
}

void TestTrash::copySymlinkFromTrash()
{
    const QString destPath = otherTmpDir() + "symlinkFromHome_copied";
    copyFromTrash( "symlinkFromHome", destPath );
    QVERIFY( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::moveFromTrash( const QString& fileId, const QString& destPath, const QString& relativePath )
{
    KUrl src( "trash:/0-" + fileId );
    if ( !relativePath.isEmpty() )
        src.addPath( relativePath );
    KUrl dest;
    dest.setPath( destPath );

    QVERIFY(MyNetAccess_exists(src));

    // A dnd would use move(), but we use moveAs to ensure the final filename
    KIO::Job* job = KIO::moveAs( src, dest, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( ok );
    QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    QVERIFY( !QFile::exists( infoFile ) );

    QFileInfo filesItem( m_trashDir + "/files/" + fileId );
    QVERIFY( !filesItem.exists() );

    QVERIFY( QFile::exists( destPath ) );
}

void TestTrash::moveFileFromTrash()
{
    const QString destPath = otherTmpDir() + "fileFromHome_restored";
    moveFromTrash( "fileFromHome", destPath );
    QVERIFY( QFileInfo( destPath ).isFile() );
    QVERIFY( QFileInfo( destPath ).size() == 12 );

    // trash it again for later
    const QString fileName = "fileFromHome";
    trashFile( homeTmpDir() + fileName, fileName );
}

void TestTrash::moveFileInDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + "testfile_restored";
    copyFromTrash( "trashDirFromHome", destPath, "testfile" );
    QVERIFY( QFileInfo( destPath ).isFile() );
    QVERIFY( QFileInfo( destPath ).size() == 12 );
}

void TestTrash::moveDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + "trashDirFromHome_restored";
    moveFromTrash( "trashDirFromHome", destPath );
    QVERIFY( QFileInfo( destPath ).isDir() );

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
    QVERIFY( !ok );
    const int err = KIO::NetAccess::lastError();
    QVERIFY( err == KIO::ERR_ACCESS_DENIED
            || err == KIO::ERR_CANNOT_OPEN_FOR_READING );

    const QString infoPath( m_trashDir + "/info/" + fileId + ".trashinfo" );
    QVERIFY( !QFile::exists( infoPath ) );

    QFileInfo files( m_trashDir + "/files/" + fileId );
    QVERIFY( !files.exists() );

    QVERIFY( QFile::exists( u.path() ) );
}

void TestTrash::moveSymlinkFromTrash()
{
    const QString destPath = otherTmpDir() + "symlinkFromHome_restored";
    moveFromTrash( "symlinkFromHome", destPath );
    QVERIFY( QFileInfo( destPath ).isSymLink() );
}

void TestTrash::getFile()
{
    const QString fileId = "fileFromHome_1";
    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );

    KTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QString tmpFilePath = tmpFile.fileName();

    KIO::Job* getJob = KIO::file_copy(url, KUrl(tmpFilePath), -1, KIO::Overwrite | KIO::HideProgressInfo);
    bool ok = KIO::NetAccess::synchronousRun(getJob, 0);
    if (!ok) {
        kDebug() << getJob->errorString();
    }
    QVERIFY( ok );
    // Don't use tmpFile.close()+tmpFile.open() here, the size would still be 0 in the QTemporaryFile object
    // (due to the use of fstat on the old fd). Arguably a bug (I even have a testcase), but probably
    // not fixable without breaking the security of QTemporaryFile...
    QFile reader(tmpFilePath);
    QVERIFY(reader.open(QIODevice::ReadOnly));
    QByteArray str = reader.readAll();
    QCOMPARE(str, QByteArray("Hello world\n"));
}

void TestTrash::restoreFile()
{
    const QString fileId = "fileFromHome_1";
    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    const QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    const QString filesItem( m_trashDir + "/files/" + fileId );

    QVERIFY( QFile::exists( infoFile ) );
    QVERIFY( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
    stream << (int)3 << url;
    KIO::Job* job = KIO::special( url, packedArgs, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( ok );

    QVERIFY( !QFile::exists( infoFile ) );
    QVERIFY( !QFile::exists( filesItem ) );

    const QString destPath = homeTmpDir() + "fileFromHome";
    QVERIFY( QFile::exists( destPath ) );
}

void TestTrash::restoreFileFromSubDir()
{
    const QString fileId = "trashDirFromHome_1/testfile";
    QVERIFY( !QFile::exists( homeTmpDir() + "trashDirFromHome_1" ) );

    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    const QString infoFile( m_trashDir + "/info/trashDirFromHome_1.trashinfo" );
    const QString filesItem( m_trashDir + "/files/trashDirFromHome_1/testfile" );

    QVERIFY( QFile::exists( infoFile ) );
    QVERIFY( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
    stream << (int)3 << url;
    KIO::Job* job = KIO::special( url, packedArgs, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( !ok );
    // dest dir doesn't exist -> error message
    QVERIFY( KIO::NetAccess::lastError() == KIO::ERR_SLAVE_DEFINED );

    // check that nothing happened
    QVERIFY( QFile::exists( infoFile ) );
    QVERIFY( QFile::exists( filesItem ) );
    QVERIFY( !QFile::exists( homeTmpDir() + "trashDirFromHome_1" ) );
}

void TestTrash::restoreFileToDeletedDirectory()
{
    // Ensure we'll get "fileFromHome" as fileId
    removeFile( m_trashDir, "/info/fileFromHome.trashinfo" );
    removeFile( m_trashDir, "/files/fileFromHome" );
    trashFileFromHome();
    // Delete orig dir
    KIO::Job* delJob = KIO::del(KUrl(homeTmpDir()), KIO::HideProgressInfo);
    bool delOK = KIO::NetAccess::synchronousRun(delJob, 0);
    QVERIFY( delOK );

    const QString fileId = "fileFromHome";
    const KUrl url = TrashImpl::makeURL( 0, fileId, QString() );
    const QString infoFile( m_trashDir + "/info/" + fileId + ".trashinfo" );
    const QString filesItem( m_trashDir + "/files/" + fileId );

    QVERIFY( QFile::exists( infoFile ) );
    QVERIFY( QFile::exists( filesItem ) );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
    stream << (int)3 << url;
    KIO::Job* job = KIO::special( url, packedArgs, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( !ok );
    // dest dir doesn't exist -> error message
    QVERIFY( KIO::NetAccess::lastError() == KIO::ERR_SLAVE_DEFINED );

    // check that nothing happened
    QVERIFY( QFile::exists( infoFile ) );
    QVERIFY( QFile::exists( filesItem ) );

    const QString destPath = homeTmpDir() + "fileFromHome";
    QVERIFY( !QFile::exists( destPath ) );
}

void TestTrash::listRootDir()
{
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( KUrl("trash:/"), KIO::HideProgressInfo );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount;
    QVERIFY( m_entryCount > 1 );

    kDebug() << m_listResult;
    QVERIFY( m_listResult.contains( "." ) == 1 ); // found it, and only once
}

void TestTrash::listRecursiveRootDir()
{
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listRecursive( KUrl("trash:/"), KIO::HideProgressInfo );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount;
    QVERIFY( m_entryCount > 1 );

    kDebug() << m_listResult;
    QVERIFY( m_listResult.count( "." ) == 1 ); // found it, and only once
}

void TestTrash::listSubDir()
{
    m_entryCount = 0;
    m_listResult.clear();
    KIO::ListJob* job = KIO::listDir( KUrl("trash:/0-trashDirFromHome"), KIO::HideProgressInfo );
    connect( job, SIGNAL( entries( KIO::Job*, const KIO::UDSEntryList& ) ),
             SLOT( slotEntries( KIO::Job*, const KIO::UDSEntryList& ) ) );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( ok );
    kDebug() << "listDir done - m_entryCount=" << m_entryCount;
    QVERIFY( m_entryCount == 2 );

    kDebug() << m_listResult;
    QVERIFY( m_listResult.count( "." ) == 1 ); // found it, and only once
    QVERIFY( m_listResult.count( "testfile" ) == 1 ); // found it, and only once
}

void TestTrash::slotEntries( KIO::Job*, const KIO::UDSEntryList& lst )
{
    for( KIO::UDSEntryList::ConstIterator it = lst.begin(); it != lst.end(); ++it ) {
        const KIO::UDSEntry& entry (*it);
        QString displayName = entry.stringValue( KIO::UDSEntry::UDS_NAME );
        KUrl url = entry.stringValue( KIO::UDSEntry::UDS_URL );
        kDebug() << displayName << " " << url;
        if ( !url.isEmpty() ) {
            QVERIFY( url.protocol() == "trash" );
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

    // To make this test standalone
    trashFileFromHome();

    // #167051: orphaned files
    createTestFile( m_trashDir + "/files/testfile_nometadata" );

    QByteArray packedArgs;
    QDataStream stream( &packedArgs, QIODevice::WriteOnly );
    stream << (int)1;
    KIO::Job* job = KIO::special( KUrl( "trash:/" ), packedArgs, KIO::HideProgressInfo );
    bool ok = KIO::NetAccess::synchronousRun( job, 0 );
    QVERIFY( ok );

    KConfig cfg( "trashrc", KConfig::SimpleConfig );
    QVERIFY( cfg.hasGroup( "Status" ) );
    QVERIFY( cfg.group("Status").readEntry( "Empty", false ) == true );

    QVERIFY( !QFile::exists( m_trashDir + "/files/fileFromHome" ) );
    QVERIFY( !QFile::exists( m_trashDir + "/files/readonly" ) );
    QVERIFY( !QFile::exists( m_trashDir + "/info/readonly.trashinfo" ) );
    QVERIFY(QDir(m_trashDir + "/info").entryList(QDir::NoDotAndDotDot|QDir::AllEntries).isEmpty());
    QVERIFY(QDir(m_trashDir + "/files").entryList(QDir::NoDotAndDotDot|QDir::AllEntries).isEmpty());

#else
    kDebug() << " : SKIPPED";
#endif
}

static void checkIcon( const KUrl& url, const QString& expectedIcon )
{
    QString icon = KMimeType::iconNameForUrl( url );
    QCOMPARE( icon, expectedIcon );
}

void TestTrash::testIcons()
{
    checkIcon( KUrl("trash:/"), "user-trash-full" ); // #100321
    checkIcon( KUrl("trash:/foo/"), "inode-directory" );
}

QTEST_KDEMAIN(TestTrash, NoGUI)

#include "testtrash.moc"
