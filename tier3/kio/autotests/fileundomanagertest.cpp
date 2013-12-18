/* This file is part of KDE
    Copyright (c) 2006, 2008 David Faure <faure@kde.org>

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

#include <qtest.h>

#include "fileundomanagertest.h"
#include <QSignalSpy>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <qplatformdefs.h>
#include <kio/fileundomanager.h>

#include <kio/copyjob.h>
#include <kio/job.h>
#include <kio/deletejob.h>
#include <kio/paste.h>
#include <kprotocolinfo.h>
#include <kurlmimedata.h>

#include <QDebug>
#include <kconfig.h>
#include <kconfiggroup.h>

#include <errno.h>
#include <utime.h>
#include <time.h>
#include <sys/time.h>

#include <QClipboard>
#include <QApplication>
#include <QMimeData>


QTEST_MAIN(FileUndoManagerTest)

using namespace KIO;

static QString homeTmpDir() { return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + '/'; }
static QString destDir() { return homeTmpDir() + "destdir/"; }

static QString srcFile() { return homeTmpDir() + "testfile"; }
static QString destFile() { return destDir() + "testfile"; }

#ifndef Q_OS_WIN
static QString srcLink() { return homeTmpDir() + "symlink"; }
static QString destLink() { return destDir() + "symlink"; }
#endif

static QString srcSubDir() { return homeTmpDir() + "subdir"; }
static QString destSubDir() { return destDir() + "subdir"; }

static QList<QUrl> sourceList()
{
    QList<QUrl> lst;
    lst << QUrl::fromLocalFile(srcFile());
#ifndef Q_OS_WIN
    lst << QUrl::fromLocalFile(srcLink());
#endif
    return lst;
}

static void createTestFile( const QString& path, const char* contents )
{
    QFile f( path );
    if ( !f.open( QIODevice::WriteOnly ) )
        qFatal("Couldn't create %s", qPrintable(path));
    f.write( QByteArray( contents ) );
    f.close();
}

static void createTestSymlink( const QString& path )
{
    // Create symlink if it doesn't exist yet
    QT_STATBUF buf;
    if (QT_LSTAT(QFile::encodeName(path).constData(), &buf) != 0) {
        bool ok = symlink("/IDontExist", QFile::encodeName(path).constData()) == 0; // broken symlink
        if ( !ok )
            qFatal("couldn't create symlink: %s", strerror(errno));
        QVERIFY(QT_LSTAT(QFile::encodeName(path).constData(), &buf) == 0);
        QVERIFY( (buf.st_mode & QT_STAT_MASK) == QT_STAT_LNK );
    } else {
        QVERIFY( (buf.st_mode & QT_STAT_MASK) == QT_STAT_LNK );
    }
    qDebug( "symlink %s created", qPrintable( path ) );
    QVERIFY( QFileInfo( path ).isSymLink() );
}

static void checkTestDirectory( const QString& path )
{
    QVERIFY( QFileInfo( path ).isDir() );
    QVERIFY( QFileInfo( path + "/fileindir" ).isFile() );
#ifndef Q_OS_WIN
    QVERIFY( QFileInfo( path + "/testlink" ).isSymLink() );
#endif
    QVERIFY( QFileInfo( path + "/dirindir" ).isDir() );
    QVERIFY( QFileInfo( path + "/dirindir/nested" ).isFile() );
}

static void createTestDirectory( const QString& path )
{
    QDir dir;
    bool ok = dir.mkdir( path );
    if ( !ok )
        qFatal("couldn't create %s", qPrintable(path));
    createTestFile( path + "/fileindir", "File in dir" );
#ifndef Q_OS_WIN
    createTestSymlink( path + "/testlink" );
#endif
    ok = dir.mkdir( path + "/dirindir" );
    if ( !ok )
        qFatal("couldn't create %s", qPrintable(path));
    createTestFile( path + "/dirindir/nested", "Nested" );
    checkTestDirectory( path );
}

class TestUiInterface : public FileUndoManager::UiInterface
{
public:
    TestUiInterface() : FileUndoManager::UiInterface(), m_nextReplyToConfirmDeletion(true) {
        setShowProgressInfo( false );
    }
    virtual void jobError( KIO::Job* job ) {
        qFatal("%s", qPrintable(job->errorString()));
    }
    virtual bool copiedFileWasModified( const QUrl& src, const QUrl& dest, const QDateTime& srcTime, const QDateTime& destTime ) {
        Q_UNUSED( src );
        m_dest = dest;
        Q_UNUSED( srcTime );
        Q_UNUSED( destTime );
        return true;
    }
    virtual bool confirmDeletion(const QList<QUrl> &files) {
        m_files = files;
        return m_nextReplyToConfirmDeletion;
    }
    void setNextReplyToConfirmDeletion( bool b ) {
        m_nextReplyToConfirmDeletion = b;
    }
    QList<QUrl> files() const { return m_files; }
    QUrl dest() const { return m_dest; }
    void clear() {
        m_dest = QUrl();
        m_files.clear();
    }
private:
    bool m_nextReplyToConfirmDeletion;
    QUrl m_dest;
    QList<QUrl> m_files;
};

void FileUndoManagerTest::initTestCase()
{
    qDebug( "initTestCase" );

    // TODO: needs QStandardPaths::isTestModeEnabled() in ksycoca.cpp when launching kbuildsycoca
    //QStandardPaths::enableTestMode(true);

    // Get kio_trash to share our environment so that it writes trashrc to the right kdehome
    qputenv( "KDE_FORK_SLAVES", "yes");

    // Start with a clean base dir
    cleanupTestCase();

    QDir dir; // TT: why not a static method?
    if ( !QFile::exists( homeTmpDir() ) ) {
        bool ok = dir.mkdir( homeTmpDir() );
        if ( !ok )
            qFatal("Couldn't create %s", qPrintable(homeTmpDir()));
    }

    createTestFile( srcFile(), "Hello world" );
#ifndef Q_OS_WIN
    createTestSymlink( srcLink() );
#endif
    createTestDirectory( srcSubDir() );

    QDir().mkdir( destDir() );
    QVERIFY( QFileInfo( destDir() ).isDir() );

    QVERIFY( !FileUndoManager::self()->undoAvailable() );
    m_uiInterface = new TestUiInterface; // owned by FileUndoManager
    FileUndoManager::self()->setUiInterface( m_uiInterface );
}

void FileUndoManagerTest::cleanupTestCase()
{
    KIO::Job* job = KIO::del(QUrl::fromLocalFile( homeTmpDir() ), KIO::HideProgressInfo);
    job->exec();
}

void FileUndoManagerTest::doUndo()
{
    QEventLoop eventLoop;
    bool ok = connect( FileUndoManager::self(), SIGNAL(undoJobFinished()),
                  &eventLoop, SLOT(quit()) );
    QVERIFY( ok );

    FileUndoManager::self()->undo();
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents); // wait for undo job to finish
}

void FileUndoManagerTest::testCopyFiles()
{
    qDebug() ;
    // Initially inspired from JobTest::copyFileToSamePartition()
    const QString destdir = destDir();
    QList<QUrl> lst = sourceList();
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob* job = KIO::copy( lst, d, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordCopyJob(job);

    QSignalSpy spyUndoAvailable( FileUndoManager::self(), SIGNAL(undoAvailable(bool)) );
    QVERIFY( spyUndoAvailable.isValid() );
    QSignalSpy spyTextChanged( FileUndoManager::self(), SIGNAL(undoTextChanged(QString)) );
    QVERIFY( spyTextChanged.isValid() );

    bool ok = job->exec();
    QVERIFY( ok );

    QVERIFY( QFile::exists( destFile() ) );
#ifndef Q_OS_WIN
    // Don't use QFile::exists, it's a broken symlink...
    QVERIFY( QFileInfo( destLink() ).isSymLink() );
#endif

    // might have to wait for dbus signal here... but this is currently disabled.
    //QTest::qWait( 20 );
    QVERIFY( FileUndoManager::self()->undoAvailable() );
    QCOMPARE( spyUndoAvailable.count(), 1 );
    QCOMPARE( spyTextChanged.count(), 1 );
    m_uiInterface->clear();

    m_uiInterface->setNextReplyToConfirmDeletion( false ); // act like the user didn't confirm
    FileUndoManager::self()->undo();
    QCOMPARE( m_uiInterface->files().count(), 1 ); // confirmDeletion was called
    QCOMPARE(m_uiInterface->files()[0].toString(), QUrl::fromLocalFile(destFile()).toString());
    QVERIFY( QFile::exists( destFile() ) ); // nothing happened yet

    // OK, now do it
    m_uiInterface->clear();
    m_uiInterface->setNextReplyToConfirmDeletion( true );
    doUndo();

    QVERIFY( !FileUndoManager::self()->undoAvailable() );
    QVERIFY( spyUndoAvailable.count() >= 2 ); // it's in fact 3, due to lock/unlock emitting it as well
    QCOMPARE( spyTextChanged.count(), 2 );
    QCOMPARE( m_uiInterface->files().count(), 1 ); // confirmDeletion was called
    QCOMPARE(m_uiInterface->files()[0].toString(), QUrl::fromLocalFile(destFile()).toString());

    // Check that undo worked
    QVERIFY( !QFile::exists( destFile() ) );
#ifndef Q_OS_WIN
    QVERIFY( !QFile::exists( destLink() ) );
    QVERIFY( !QFileInfo( destLink() ).isSymLink() );
#endif
}

void FileUndoManagerTest::testMoveFiles()
{
    qDebug() ;
    const QString destdir = destDir();
    QList<QUrl> lst = sourceList();
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob* job = KIO::move( lst, d, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordCopyJob(job);

    bool ok = job->exec();
    QVERIFY( ok );

    QVERIFY( !QFile::exists( srcFile() ) ); // the source moved
    QVERIFY( QFile::exists( destFile() ) );
#ifndef Q_OS_WIN
    QVERIFY( !QFileInfo( srcLink() ).isSymLink() );
    // Don't use QFile::exists, it's a broken symlink...
    QVERIFY( QFileInfo( destLink() ).isSymLink() );
#endif

    doUndo();

    QVERIFY( QFile::exists( srcFile() ) ); // the source is back
    QVERIFY( !QFile::exists( destFile() ) );
#ifndef Q_OS_WIN
    QVERIFY( QFileInfo( srcLink() ).isSymLink() );
    QVERIFY( !QFileInfo( destLink() ).isSymLink() );
#endif
}

// Testing for overwrite isn't possible, because non-interactive jobs never overwrite.
// And nothing different happens anyway, the dest is removed...
#if 0
void FileUndoManagerTest::testCopyFilesOverwrite()
{
    qDebug() ;
    // Create a different file in the destdir
    createTestFile( destFile(), "An old file already in the destdir" );

    testCopyFiles();
}
#endif

void FileUndoManagerTest::testCopyDirectory()
{
    const QString destdir = destDir();
    QList<QUrl> lst; lst << QUrl::fromLocalFile(srcSubDir());
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob* job = KIO::copy( lst, d, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordCopyJob(job);

    bool ok = job->exec();
    QVERIFY( ok );

    checkTestDirectory( srcSubDir() ); // src untouched
    checkTestDirectory( destSubDir() );

    doUndo();

    checkTestDirectory( srcSubDir() );
    QVERIFY( !QFile::exists( destSubDir() ) );
}

void FileUndoManagerTest::testMoveDirectory()
{
    const QString destdir = destDir();
    QList<QUrl> lst; lst << QUrl::fromLocalFile(srcSubDir());
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob* job = KIO::move( lst, d, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordCopyJob(job);

    bool ok = job->exec();
    QVERIFY( ok );

    QVERIFY( !QFile::exists( srcSubDir() ) );
    checkTestDirectory( destSubDir() );

    doUndo();

    checkTestDirectory( srcSubDir() );
    QVERIFY( !QFile::exists( destSubDir() ) );
}

void FileUndoManagerTest::testRenameFile()
{
    const QUrl oldUrl = QUrl::fromLocalFile( srcFile() );
    const QUrl newUrl = QUrl::fromLocalFile( srcFile() + ".new" );
    QList<QUrl> lst;
    lst.append(oldUrl);
    QSignalSpy spyUndoAvailable( FileUndoManager::self(), SIGNAL(undoAvailable(bool)) );
    QVERIFY( spyUndoAvailable.isValid() );
    KIO::Job* job = KIO::moveAs( oldUrl, newUrl, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordJob( FileUndoManager::Rename, lst, newUrl, job );

    bool ok = job->exec();
    QVERIFY( ok );

    QVERIFY( !QFile::exists( srcFile() ) );
    QVERIFY( QFileInfo( newUrl.toLocalFile() ).isFile() );
    QCOMPARE(spyUndoAvailable.count(), 1);

    doUndo();

    QVERIFY( QFile::exists( srcFile() ) );
    QVERIFY( !QFileInfo( newUrl.toLocalFile() ).isFile() );
}

void FileUndoManagerTest::testRenameDir()
{
    const QUrl oldUrl = QUrl::fromLocalFile( srcSubDir() );
    const QUrl newUrl = QUrl::fromLocalFile( srcSubDir() + ".new" );
    QList<QUrl> lst;
    lst.append(oldUrl);
    KIO::Job* job = KIO::moveAs( oldUrl, newUrl, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordJob( FileUndoManager::Rename, lst, newUrl, job );

    bool ok = job->exec();
    QVERIFY( ok );

    QVERIFY( !QFile::exists( srcSubDir() ) );
    QVERIFY( QFileInfo( newUrl.toLocalFile() ).isDir() );

    doUndo();

    QVERIFY( QFile::exists( srcSubDir() ) );
    QVERIFY( !QFileInfo( newUrl.toLocalFile() ).isDir() );
}

void FileUndoManagerTest::testCreateDir()
{
    const QUrl url = QUrl::fromLocalFile(srcSubDir() + ".mkdir");
    const QString path = url.toLocalFile();
    QVERIFY( !QFile::exists(path) );

    KIO::SimpleJob* job = KIO::mkdir(url);
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordJob( FileUndoManager::Mkdir, QList<QUrl>(), url, job );
    bool ok = job->exec();
    QVERIFY( ok );
    QVERIFY( QFile::exists(path) );
    QVERIFY( QFileInfo(path).isDir() );

    m_uiInterface->clear();
    m_uiInterface->setNextReplyToConfirmDeletion( false ); // act like the user didn't confirm
    FileUndoManager::self()->undo();
    QCOMPARE( m_uiInterface->files().count(), 1 ); // confirmDeletion was called
    QCOMPARE(m_uiInterface->files()[0].toString(), url.toString());
    QVERIFY( QFile::exists(path) ); // nothing happened yet

    // OK, now do it
    m_uiInterface->clear();
    m_uiInterface->setNextReplyToConfirmDeletion( true );
    doUndo();

    QVERIFY( !QFile::exists(path) );
}

void FileUndoManagerTest::testTrashFiles()
{
    if ( !KProtocolInfo::isKnownProtocol( "trash" ) )
        QSKIP( "kio_trash not installed" );

    // Trash it all at once: the file, the symlink, the subdir.
    QList<QUrl> lst = sourceList();
    lst.append(QUrl::fromLocalFile(srcSubDir()));
    KIO::Job* job = KIO::trash( lst, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordJob( FileUndoManager::Trash, lst, QUrl("trash:/"), job );

    bool ok = job->exec();
    QVERIFY( ok );

    // Check that things got removed
    QVERIFY( !QFile::exists( srcFile() ) );
#ifndef Q_OS_WIN
    QVERIFY( !QFileInfo( srcLink() ).isSymLink() );
#endif
    QVERIFY( !QFile::exists( srcSubDir() ) );

    // check trash?
    // Let's just check that it's not empty. kio_trash has its own unit tests anyway.
    KConfig cfg( "trashrc", KConfig::SimpleConfig );
    QVERIFY( cfg.hasGroup( "Status" ) );
    QCOMPARE( cfg.group("Status").readEntry( "Empty", true ), false );

    doUndo();

    QVERIFY( QFile::exists( srcFile() ) );
#ifndef Q_OS_WIN
    QVERIFY( QFileInfo( srcLink() ).isSymLink() );
#endif
    QVERIFY( QFile::exists( srcSubDir() ) );

    // We can't check that the trash is empty; other partitions might have their own trash
}

static void setTimeStamp( const QString& path )
{
#ifdef Q_OS_UNIX
    // Put timestamp in the past so that we can check that the
    // copy actually preserves it.
    struct timeval tp;
    gettimeofday( &tp, 0 );
    struct utimbuf utbuf;
    utbuf.actime = tp.tv_sec + 30; // 30 seconds in the future
    utbuf.modtime = tp.tv_sec + 60; // 60 second in the future
    utime(QFile::encodeName(path).constData(), &utbuf);
    qDebug( "Time changed for %s", qPrintable( path ) );
#endif
}

void FileUndoManagerTest::testModifyFileBeforeUndo()
{
    // based on testCopyDirectory (so that we check that it works for files in subdirs too)
    const QString destdir = destDir();
    QList<QUrl> lst; lst << QUrl::fromLocalFile(srcSubDir());
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob* job = KIO::copy( lst, d, KIO::HideProgressInfo );
    job->setUiDelegate( 0 );
    FileUndoManager::self()->recordCopyJob(job);

    bool ok = job->exec();
    QVERIFY( ok );

    checkTestDirectory( srcSubDir() ); // src untouched
    checkTestDirectory( destSubDir() );
    const QString destFile =  destSubDir() + "/fileindir";
    setTimeStamp( destFile ); // simulate a modification of the file

    doUndo();

    // Check that TestUiInterface::copiedFileWasModified got called
    QCOMPARE( m_uiInterface->dest().toLocalFile(), destFile );

    checkTestDirectory( srcSubDir() );
    QVERIFY( !QFile::exists( destSubDir() ) );
}

void FileUndoManagerTest::testPasteClipboardUndo()
{
    const QList<QUrl> urls (sourceList());
    QMimeData* mimeData = new QMimeData();
    mimeData->setUrls(urls);
    mimeData->setData(QLatin1String("application/x-kde-cutselection"), "1");
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setMimeData(mimeData);

    // Paste the contents of the clipboard and check its status
    QUrl destDirUrl = QUrl::fromLocalFile(destDir());
    KIO::CopyJob* job = qobject_cast<KIO::CopyJob*>(KIO::pasteClipboard(destDirUrl, 0, true));
    QVERIFY(job);
    FileUndoManager::self()->recordCopyJob(job);
    QVERIFY(job->exec());

    // Check if the clipboard was updated after paste operation
    QList<QUrl> urls2;
    Q_FOREACH(const QUrl& url, urls) {
        QUrl dUrl = destDirUrl.adjusted(QUrl::StripTrailingSlash);
        dUrl.setPath(dUrl.path() + '/' + url.fileName());
        urls2 << dUrl;
    }
    QList<QUrl> clipboardUrls = KUrlMimeData::urlsFromMimeData(clipboard->mimeData());
    QCOMPARE(clipboardUrls, urls2);

    // Check if the clipboard was updated after undo operation
    doUndo();
    clipboardUrls = KUrlMimeData::urlsFromMimeData(clipboard->mimeData());
    QCOMPARE(clipboardUrls, urls);
}


// TODO: add test (and fix bug) for  DND of remote urls / "Link here" (creates .desktop files) // Undo (doesn't do anything)
// TODO: add test for interrupting a moving operation and then using Undo - bug:91579
