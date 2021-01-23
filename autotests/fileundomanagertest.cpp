/*
    This file is part of KDE
    SPDX-FileCopyrightText: 2006, 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "fileundomanagertest.h"
#include "mockcoredelegateextensions.h"

#include <QTest>
#include <QSignalSpy>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <qplatformdefs.h>
#include <kio/fileundomanager.h>

#include <kio/copyjob.h>
#include <kio/job.h>
#include <kio/mkpathjob.h>
#include <kio/deletejob.h>
#include <kio/paste.h>
#include <kio/pastejob.h>
#include <kio/restorejob.h>
#include <kio/batchrenamejob.h>
#include <kioglobal_p.h>
#include <kprotocolinfo.h>
#include <KUrlMimeData>

#include <QDebug>
#include <KConfig>
#include <KConfigGroup>

#include <errno.h>
#include <time.h>
#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#include <sys/time.h>
#endif

#include <QClipboard>
#include <QApplication>
#include <QMimeData>

QTEST_MAIN(FileUndoManagerTest)

using namespace KIO;

static QString homeTmpDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QDir::separator();
}
static QString destDir()
{
    return homeTmpDir() + "destdir/";
}

static QString srcFile()
{
    return homeTmpDir() + "testfile";
}
static QString destFile()
{
    return destDir() + "testfile";
}

#ifndef Q_OS_WIN
static QString srcLink()
{
    return homeTmpDir() + "symlink";
}
static QString destLink()
{
    return destDir() + "symlink";
}
#endif

static QString srcSubDir()
{
    return homeTmpDir() + "subdir";
}
static QString destSubDir()
{
    return destDir() + "subdir";
}

static QList<QUrl> sourceList()
{
    QList<QUrl> lst;
    lst << QUrl::fromLocalFile(srcFile());
#ifndef Q_OS_WIN
    lst << QUrl::fromLocalFile(srcLink());
#endif
    return lst;
}

static void createTestFile(const QString &path, const char *contents)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qFatal("Couldn't create %s", qPrintable(path));
    }
    f.write(QByteArray(contents));
    f.close();
}

static void createTestSymlink(const QString &path)
{
    // Create symlink if it doesn't exist yet
    QT_STATBUF buf;
    if (QT_LSTAT(QFile::encodeName(path).constData(), &buf) != 0) {
        bool ok = KIOPrivate::createSymlink(QStringLiteral("/IDontExist"), path); // broken symlink
        if (!ok) {
            qFatal("couldn't create symlink: %s", strerror(errno));
        }
        QVERIFY(QT_LSTAT(QFile::encodeName(path).constData(), &buf) == 0);
        QVERIFY((buf.st_mode & QT_STAT_MASK) == QT_STAT_LNK);
    } else {
        QVERIFY((buf.st_mode & QT_STAT_MASK) == QT_STAT_LNK);
    }
    qDebug("symlink %s created", qPrintable(path));
    QVERIFY(QFileInfo(path).isSymLink());
}

static void checkTestDirectory(const QString &path)
{
    QVERIFY(QFileInfo(path).isDir());
    QVERIFY(QFileInfo(path + "/fileindir").isFile());
#ifndef Q_OS_WIN
    QVERIFY(QFileInfo(path + "/testlink").isSymLink());
#endif
    QVERIFY(QFileInfo(path + "/dirindir").isDir());
    QVERIFY(QFileInfo(path + "/dirindir/nested").isFile());
}

static void createTestDirectory(const QString &path)
{
    QDir dir;
    bool ok = dir.mkpath(path);
    if (!ok) {
        qFatal("couldn't create %s", qPrintable(path));
    }
    createTestFile(path + "/fileindir", "File in dir");
#ifndef Q_OS_WIN
    createTestSymlink(path + "/testlink");
#endif
    ok = dir.mkdir(path + "/dirindir");
    if (!ok) {
        qFatal("couldn't create %s", qPrintable(path));
    }
    createTestFile(path + "/dirindir/nested", "Nested");
    checkTestDirectory(path);
}

class TestUiInterface : public FileUndoManager::UiInterface
{
public:
    TestUiInterface() : FileUndoManager::UiInterface()
      , m_mockAskUserInterface(new MockAskUserInterface)
    {
        setShowProgressInfo(false);
    }
    void jobError(KIO::Job *job) override {
        m_errorCode = job->error();
        qWarning() << job->errorString();
    }
    bool copiedFileWasModified(const QUrl &src, const QUrl &dest, const QDateTime &srcTime, const QDateTime &destTime) override {
        Q_UNUSED(src);
        m_dest = dest;
        Q_UNUSED(srcTime);
        Q_UNUSED(destTime);
        return true;
    }
    bool confirmDeletion(const QList<QUrl> &) override {
        Q_ASSERT(false); // no longer called
        return false;
    }
    void setNextReplyToConfirmDeletion(bool b)
    {
        m_nextReplyToConfirmDeletion = b;
    }
    QUrl dest() const
    {
        return m_dest;
    }
    int errorCode() const
    {
        return m_errorCode;
    }
    void clear()
    {
        m_dest = QUrl();
        m_errorCode = 0;
        m_mockAskUserInterface->clear();
    }
    MockAskUserInterface* askUserMockInterface() const
    {
        return m_mockAskUserInterface.get();
    }
    void virtual_hook(int id, void *data) override
    {
        if (id == HookGetAskUserActionInterface) {
            AskUserActionInterface** p = static_cast<AskUserActionInterface**>(data);
            *p = m_mockAskUserInterface.get();
            m_mockAskUserInterface->m_deleteResult = m_nextReplyToConfirmDeletion;
        }
    }

private:
    QUrl m_dest;
    std::unique_ptr<MockAskUserInterface> m_mockAskUserInterface;
    int m_errorCode = 0;
    bool m_nextReplyToConfirmDeletion = true;
};

void FileUndoManagerTest::initTestCase()
{
    qDebug("initTestCase");

    QStandardPaths::setTestModeEnabled(true);

    // Get kio_trash to share our environment so that it writes trashrc to the right kdehome
    qputenv("KDE_FORK_SLAVES", "yes");
    qputenv("KIOSLAVE_ENABLE_TESTMODE", "1");

    // Start with a clean base dir
    cleanupTestCase();

    if (!QFile::exists(homeTmpDir())) {
        bool ok = QDir().mkpath(homeTmpDir());
        if (!ok) {
            qFatal("Couldn't create %s", qPrintable(homeTmpDir()));
        }
    }

    createTestFile(srcFile(), "Hello world");
#ifndef Q_OS_WIN
    createTestSymlink(srcLink());
#endif
    createTestDirectory(srcSubDir());

    QDir().mkpath(destDir());
    QVERIFY(QFileInfo(destDir()).isDir());

    QVERIFY(!FileUndoManager::self()->isUndoAvailable());
    m_uiInterface = new TestUiInterface; // owned by FileUndoManager
    FileUndoManager::self()->setUiInterface(m_uiInterface);
}

void FileUndoManagerTest::cleanupTestCase()
{
    KIO::Job *job = KIO::del(QUrl::fromLocalFile(homeTmpDir()), KIO::HideProgressInfo);
    job->exec();
}

void FileUndoManagerTest::doUndo()
{
    QEventLoop eventLoop;
    connect(FileUndoManager::self(), &FileUndoManager::undoJobFinished,
            &eventLoop, &QEventLoop::quit);
    FileUndoManager::self()->undo();
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents); // wait for undo job to finish
}

void FileUndoManagerTest::testCopyFiles()
{
    // Initially inspired from JobTest::copyFileToSamePartition()
    const QString destdir = destDir();
    QList<QUrl> lst = sourceList();
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob *job = KIO::copy(lst, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);

    QSignalSpy spyUndoAvailable(FileUndoManager::self(), QOverload<bool>::of(&FileUndoManager::undoAvailable));
    QVERIFY(spyUndoAvailable.isValid());
    QSignalSpy spyTextChanged(FileUndoManager::self(), &FileUndoManager::undoTextChanged);
    QVERIFY(spyTextChanged.isValid());

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(QFile::exists(destFile()));
#ifndef Q_OS_WIN
    // Don't use QFile::exists, it's a broken symlink...
    QVERIFY(QFileInfo(destLink()).isSymLink());
#endif

    // might have to wait for dbus signal here... but this is currently disabled.
    //QTest::qWait( 20 );
    QVERIFY(FileUndoManager::self()->isUndoAvailable());
    QCOMPARE(spyUndoAvailable.count(), 1);
    QCOMPARE(spyTextChanged.count(), 1);
    m_uiInterface->clear();

    m_uiInterface->setNextReplyToConfirmDeletion(false);   // act like the user didn't confirm
    FileUndoManager::self()->undo();
    auto *lastMock = m_uiInterface->askUserMockInterface();
    QCOMPARE(lastMock->m_askUserDeleteCalled, 1);
    QVERIFY(QFile::exists(destFile()));     // nothing happened yet

    // OK, now do it
    m_uiInterface->clear();
    m_uiInterface->setNextReplyToConfirmDeletion(true);
    doUndo();

    QVERIFY(!FileUndoManager::self()->isUndoAvailable());
    QVERIFY(spyUndoAvailable.count() >= 2);   // it's in fact 3, due to lock/unlock emitting it as well
    QCOMPARE(spyTextChanged.count(), 2);
    QCOMPARE(m_uiInterface->askUserMockInterface()->m_askUserDeleteCalled, 1);

    // Check that undo worked
    QVERIFY(!QFile::exists(destFile()));
#ifndef Q_OS_WIN
    QVERIFY(!QFile::exists(destLink()));
    QVERIFY(!QFileInfo(destLink()).isSymLink());
#endif
}

void FileUndoManagerTest::testMoveFiles()
{
    const QString destdir = destDir();
    QList<QUrl> lst = sourceList();
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob *job = KIO::move(lst, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(!QFile::exists(srcFile()));     // the source moved
    QVERIFY(QFile::exists(destFile()));
#ifndef Q_OS_WIN
    QVERIFY(!QFileInfo(srcLink()).isSymLink());
    // Don't use QFile::exists, it's a broken symlink...
    QVERIFY(QFileInfo(destLink()).isSymLink());
#endif

    doUndo();

    QVERIFY(QFile::exists(srcFile()));     // the source is back
    QVERIFY(!QFile::exists(destFile()));
#ifndef Q_OS_WIN
    QVERIFY(QFileInfo(srcLink()).isSymLink());
    QVERIFY(!QFileInfo(destLink()).isSymLink());
#endif
}

void FileUndoManagerTest::testCopyDirectory()
{
    const QString destdir = destDir();
    QList<QUrl> lst; lst << QUrl::fromLocalFile(srcSubDir());
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob *job = KIO::copy(lst, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    checkTestDirectory(srcSubDir());   // src untouched
    checkTestDirectory(destSubDir());

    doUndo();

    checkTestDirectory(srcSubDir());
    QVERIFY(!QFile::exists(destSubDir()));
}

void FileUndoManagerTest::testCopyEmptyDirectory()
{
    const QString src = srcSubDir() + "/.emptydir";
    const QString destEmptyDir = destDir() + "/.emptydir";
    QDir().mkpath(src);
    KIO::CopyJob *job = KIO::copy({QUrl::fromLocalFile(src)}, QUrl::fromLocalFile(destEmptyDir), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(QFileInfo(src).isDir()); // untouched
    QVERIFY(QFileInfo(destEmptyDir).isDir());

    doUndo();

    QVERIFY(QFileInfo(src).isDir()); // untouched
    QVERIFY(!QFile::exists(destEmptyDir));
}

void FileUndoManagerTest::testMoveDirectory()
{
    const QString destdir = destDir();
    QList<QUrl> lst; lst << QUrl::fromLocalFile(srcSubDir());
    const QUrl d = QUrl::fromLocalFile(destdir);
    KIO::CopyJob *job = KIO::move(lst, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(!QFile::exists(srcSubDir()));
    checkTestDirectory(destSubDir());

    doUndo();

    checkTestDirectory(srcSubDir());
    QVERIFY(!QFile::exists(destSubDir()));
}

void FileUndoManagerTest::testRenameFile()
{
    const QUrl oldUrl = QUrl::fromLocalFile(srcFile());
    const QUrl newUrl = QUrl::fromLocalFile(srcFile() + ".new");
    QList<QUrl> lst;
    lst.append(oldUrl);
    QSignalSpy spyUndoAvailable(FileUndoManager::self(), QOverload<bool>::of(&FileUndoManager::undoAvailable));
    QVERIFY(spyUndoAvailable.isValid());
    KIO::Job *job = KIO::moveAs(oldUrl, newUrl, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordJob(FileUndoManager::Rename, lst, newUrl, job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(!QFile::exists(srcFile()));
    QVERIFY(QFileInfo(newUrl.toLocalFile()).isFile());
    QCOMPARE(spyUndoAvailable.count(), 1);

    doUndo();

    QVERIFY(QFile::exists(srcFile()));
    QVERIFY(!QFileInfo(newUrl.toLocalFile()).isFile());
}

void FileUndoManagerTest::testRenameDir()
{
    const QUrl oldUrl = QUrl::fromLocalFile(srcSubDir());
    const QUrl newUrl = QUrl::fromLocalFile(srcSubDir() + ".new");
    QList<QUrl> lst;
    lst.append(oldUrl);
    KIO::Job *job = KIO::moveAs(oldUrl, newUrl, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordJob(FileUndoManager::Rename, lst, newUrl, job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(!QFile::exists(srcSubDir()));
    QVERIFY(QFileInfo(newUrl.toLocalFile()).isDir());

    doUndo();

    QVERIFY(QFile::exists(srcSubDir()));
    QVERIFY(!QFileInfo(newUrl.toLocalFile()).isDir());
}

void FileUndoManagerTest::testCreateSymlink()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows for lack of proper symlink support");
#endif
    const QUrl link = QUrl::fromLocalFile(homeTmpDir() + "newlink");
    const QString path = link.toLocalFile();
    QVERIFY(!QFile::exists(path));

    const QUrl target = QUrl::fromLocalFile(homeTmpDir() + "linktarget");
    const QString targetPath = target.toLocalFile();
    createTestFile(targetPath, "Link's Target");
    QVERIFY(QFile::exists(targetPath));

    KIO::CopyJob *job = KIO::link(target, link, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(path));
    QVERIFY(QFileInfo(path).isSymLink());

    // For undoing symlinks no confirmation is required. We delete it straight away.
    doUndo();

    QVERIFY(!QFile::exists(path));
}

void FileUndoManagerTest::testCreateDir()
{
    const QUrl url = QUrl::fromLocalFile(srcSubDir() + ".mkdir");
    const QString path = url.toLocalFile();
    QVERIFY(!QFile::exists(path));

    KIO::SimpleJob *job = KIO::mkdir(url);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordJob(FileUndoManager::Mkdir, QList<QUrl>(), url, job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(path));
    QVERIFY(QFileInfo(path).isDir());

    m_uiInterface->clear();
    m_uiInterface->setNextReplyToConfirmDeletion(false);   // act like the user didn't confirm
    FileUndoManager::self()->undo();
    QCOMPARE(m_uiInterface->askUserMockInterface()->m_askUserDeleteCalled, 1);
    QVERIFY(QFile::exists(path));   // nothing happened yet

    // OK, now do it
    m_uiInterface->clear();
    m_uiInterface->setNextReplyToConfirmDeletion(true);
    doUndo();

    QVERIFY(!QFile::exists(path));
}

void FileUndoManagerTest::testMkpath()
{
    const QString parent = srcSubDir() + "mkpath";
    const QString path = parent + "/subdir";
    QVERIFY(!QFile::exists(path));
    const QUrl url = QUrl::fromLocalFile(path);

    KIO::Job *job = KIO::mkpath(url, QUrl(), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordJob(FileUndoManager::Mkpath, QList<QUrl>(), url, job);
    QVERIFY(job->exec());
    QVERIFY(QFileInfo(path).isDir());

    m_uiInterface->clear();
    m_uiInterface->setNextReplyToConfirmDeletion(true);
    doUndo();

    QVERIFY(!FileUndoManager::self()->isUndoAvailable());
    QCOMPARE(m_uiInterface->askUserMockInterface()->m_askUserDeleteCalled, 1);

    QVERIFY(!QFile::exists(path));
}

void FileUndoManagerTest::testTrashFiles()
{
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("trash"))) {
        QSKIP("kio_trash not installed");
    }

    // Trash it all at once: the file, the symlink, the subdir.
    QList<QUrl> lst = sourceList();
    lst.append(QUrl::fromLocalFile(srcSubDir()));
    KIO::Job *job = KIO::trash(lst, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordJob(FileUndoManager::Trash, lst, QUrl(QStringLiteral("trash:/")), job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    // Check that things got removed
    QVERIFY(!QFile::exists(srcFile()));
#ifndef Q_OS_WIN
    QVERIFY(!QFileInfo(srcLink()).isSymLink());
#endif
    QVERIFY(!QFile::exists(srcSubDir()));

    // check trash?
    // Let's just check that it's not empty. kio_trash has its own unit tests anyway.
    KConfig cfg(QStringLiteral("trashrc"), KConfig::SimpleConfig);
    QVERIFY(cfg.hasGroup("Status"));
    QCOMPARE(cfg.group("Status").readEntry("Empty", true), false);

    doUndo();

    QVERIFY(QFile::exists(srcFile()));
#ifndef Q_OS_WIN
    QVERIFY(QFileInfo(srcLink()).isSymLink());
#endif
    QVERIFY(QFile::exists(srcSubDir()));

    // We can't check that the trash is empty; other partitions might have their own trash
}

void FileUndoManagerTest::testRestoreTrashedFiles()
{
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("trash"))) {
        QSKIP("kio_trash not installed");
    }

    // Trash it all at once: the file, the symlink, the subdir.
    const QFile::Permissions origPerms = QFileInfo(srcFile()).permissions();
    QList<QUrl> lst = sourceList();
    lst.append(QUrl::fromLocalFile(srcSubDir()));
    KIO::Job *job = KIO::trash(lst, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());

    const QMap<QString, QString> metaData = job->metaData();
    QList<QUrl> trashUrls;
    for (const QUrl &src : qAsConst(lst)) {
        QMap<QString, QString>::ConstIterator it = metaData.find("trashURL-" + src.path());
        QVERIFY(it != metaData.constEnd());
        trashUrls.append(QUrl(it.value()));
    }

    // Restore from trash
    KIO::RestoreJob *restoreJob = KIO::restoreFromTrash(trashUrls, KIO::HideProgressInfo);
    restoreJob->setUiDelegate(nullptr);
    QVERIFY(restoreJob->exec());

    QVERIFY(QFile::exists(srcFile()));
    QCOMPARE(QFileInfo(srcFile()).permissions(), origPerms);
#ifndef Q_OS_WIN
    QVERIFY(QFileInfo(srcLink()).isSymLink());
#endif
    QVERIFY(QFile::exists(srcSubDir()));

    // TODO support for RestoreJob in FileUndoManager !!!
}

static void setTimeStamp(const QString &path)
{
#ifdef Q_OS_UNIX
    // Put timestamp in the past so that we can check that the
    // copy actually preserves it.
    struct timeval tp;
    gettimeofday(&tp, nullptr);
    struct utimbuf utbuf;
    utbuf.actime = tp.tv_sec + 30; // 30 seconds in the future
    utbuf.modtime = tp.tv_sec + 60; // 60 second in the future
    utime(QFile::encodeName(path).constData(), &utbuf);
    qDebug("Time changed for %s", qPrintable(path));
#endif
}

void FileUndoManagerTest::testModifyFileBeforeUndo()
{
    // based on testCopyDirectory (so that we check that it works for files in subdirs too)
    const QString destdir = destDir();
    const QList<QUrl> lst{ QUrl::fromLocalFile(srcSubDir()) };
    const QUrl dest = QUrl::fromLocalFile(destdir);
    KIO::CopyJob *job = KIO::copy(lst, dest, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    checkTestDirectory(srcSubDir());   // src untouched
    checkTestDirectory(destSubDir());
    const QString destFile = destSubDir() + "/fileindir";
    setTimeStamp(destFile);   // simulate a modification of the file

    doUndo();

    // Check that TestUiInterface::copiedFileWasModified got called
    QCOMPARE(m_uiInterface->dest().toLocalFile(), destFile);

    checkTestDirectory(srcSubDir());
    QVERIFY(!QFile::exists(destSubDir()));
}

void FileUndoManagerTest::testPasteClipboardUndo()
{
    const QList<QUrl> urls(sourceList());
    QMimeData *mimeData = new QMimeData();
    mimeData->setUrls(urls);
    KIO::setClipboardDataCut(mimeData, true);
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(mimeData);

    // Paste the contents of the clipboard and check its status
    QUrl destDirUrl = QUrl::fromLocalFile(destDir());
    KIO::Job *job = KIO::paste(mimeData, destDirUrl, KIO::HideProgressInfo);
    QVERIFY(job);
    QVERIFY(job->exec());

    // Check if the clipboard was updated after paste operation
    QList<QUrl> urls2;
    for (const QUrl &url : urls) {
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

void FileUndoManagerTest::testBatchRename()
{
    auto createUrl = [](const QString &path) -> QUrl {
        return QUrl::fromLocalFile(homeTmpDir() + path);
    };

    QList<QUrl> srcList;
    srcList << createUrl("textfile.txt") << createUrl("mediafile.mkv") << createUrl("sourcefile.cpp");

    createTestFile(srcList.at(0).path(), "foo");
    createTestFile(srcList.at(1).path(), "foo");
    createTestFile(srcList.at(2).path(), "foo");

    KIO::Job *job = KIO::batchRename(srcList, QLatin1String("newfile###"), 1, QLatin1Char('#'), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordJob(FileUndoManager::BatchRename, srcList, QUrl(), job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(QFile::exists(createUrl("newfile001.txt").path()));
    QVERIFY(QFile::exists(createUrl("newfile002.mkv").path()));
    QVERIFY(QFile::exists(createUrl("newfile003.cpp").path()));
    QVERIFY(!QFile::exists(srcList.at(0).path()));
    QVERIFY(!QFile::exists(srcList.at(1).path()));
    QVERIFY(!QFile::exists(srcList.at(2).path()));

    doUndo();

    QVERIFY(!QFile::exists(createUrl("newfile###.txt").path()));
    QVERIFY(!QFile::exists(createUrl("newfile###.mkv").path()));
    QVERIFY(!QFile::exists(createUrl("newfile###.cpp").path()));
    QVERIFY(QFile::exists(srcList.at(0).path()));
    QVERIFY(QFile::exists(srcList.at(1).path()));
    QVERIFY(QFile::exists(srcList.at(2).path()));
}

void FileUndoManagerTest::testUndoCopyOfDeletedFile()
{
    const QUrl source = QUrl::fromLocalFile(homeTmpDir() + QLatin1String("source.txt"));
    const QUrl dest = QUrl::fromLocalFile(homeTmpDir() + QLatin1String("copy.txt"));

    createTestFile(source.toLocalFile(), "foo");
    QVERIFY(QFileInfo::exists(source.toLocalFile()));

    {
        auto copyJob = KIO::copy(source, dest, KIO::HideProgressInfo);
        copyJob->setUiDelegate(nullptr);
        FileUndoManager::self()->recordCopyJob(copyJob);
        QVERIFY2(copyJob->exec(), qPrintable(copyJob->errorString()));
        QVERIFY(QFileInfo::exists(dest.toLocalFile()));
    }

    {
        auto deleteJob = KIO::del(dest, KIO::HideProgressInfo);
        deleteJob->setUiDelegate(nullptr);
        QVERIFY2(deleteJob->exec(), qPrintable(deleteJob->errorString()));
        QVERIFY(!QFileInfo::exists(dest.toLocalFile()));
    }

    QVERIFY(FileUndoManager::self()->isUndoAvailable());
    QSignalSpy spyUndoAvailable(FileUndoManager::self(), QOverload<bool>::of(&FileUndoManager::undoAvailable));
    QVERIFY(spyUndoAvailable.isValid());
    doUndo();
    QVERIFY(spyUndoAvailable.count() >= 2);   // it's in fact 3, due to lock/unlock emitting it as well
    QVERIFY(!spyUndoAvailable.at(0).at(0).toBool());
    QVERIFY(!FileUndoManager::self()->isUndoAvailable());
}

void FileUndoManagerTest::testErrorDuringMoveUndo()
{
    const QString destdir = destDir();
    QList<QUrl> lst{ QUrl::fromLocalFile(srcFile()) };
    KIO::CopyJob *job = KIO::move(lst, QUrl::fromLocalFile(destdir), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    FileUndoManager::self()->recordCopyJob(job);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(!QFile::exists(srcFile()));     // the source moved
    QVERIFY(QFile::exists(destFile()));
    createTestFile(srcFile(), "I'm back");

    doUndo();

    QCOMPARE(m_uiInterface->errorCode(), KIO::ERR_FILE_ALREADY_EXIST);
    QVERIFY(QFile::exists(destFile())); // still there
}

// TODO: add test (and fix bug) for  DND of remote urls / "Link here" (creates .desktop files) // Undo (doesn't do anything)
// TODO: add test for interrupting a moving operation and then using Undo - bug:91579
