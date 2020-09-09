/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "testtrash.h"

#include <QTest>

#include "kio_trash.h"
#include "../../../pathhelpers_p.h"

#include <kprotocolinfo.h>
#include <QTemporaryFile>
#include <QDataStream>

#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <QDebug>
#include <KConfigGroup>

#include <QDir>
#include <QUrl>
#include <QFileInfo>
#include <QVector>
#include <KJobUiDelegate>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <kfileitem.h>
#include <kio/chmodjob.h>
#include <kio/directorysizejob.h>
#include <QStandardPaths>

// There are two ways to test encoding things:
// * with utf8 filenames
// * with latin1 filenames -- not sure this still works.
//
#define UTF8TEST 1

int initLocale()
{
#ifdef UTF8TEST
    // Assume utf8 system
    setenv("LC_ALL", "C.utf-8", 1);
    setenv("KDE_UTF8_FILENAMES", "true", 1);
#else
    // Ensure a known QFile::encodeName behavior for trashUtf8FileFromHome
    // However this assume your $HOME doesn't use characters from other locales...
    setenv("LC_ALL", "en_US.ISO-8859-1", 1);
    unsetenv("KDE_UTF8_FILENAMES");
#endif
    setenv("KIOSLAVE_ENABLE_TESTMODE", "1", 1); // ensure the ioslaves call QStandardPaths::setTestModeEnabled(true) too
    setenv("KDE_SKIP_KDERC", "1", 1);
    unsetenv("KDE_COLOR_DEBUG");
    return 0;
}
Q_CONSTRUCTOR_FUNCTION(initLocale)

QString TestTrash::homeTmpDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/testtrash/");
}

QString TestTrash::readOnlyDirPath() const
{
    return homeTmpDir() + QLatin1String("readonly");
}

QString TestTrash::otherTmpDir() const
{
    // This one needs to be on another partition for the test to be meaningful
    QString tempDir = m_tempDir.path();
    if (!tempDir.endsWith(QLatin1Char('/'))) {
        tempDir.append(QLatin1Char('/'));
    }
    return tempDir;
}

QString TestTrash::utf8FileName() const
{
    return QLatin1String("test") + QChar(0x2153);     // "1/3" character, not part of latin1
}

QString TestTrash::umlautFileName() const
{
    return QLatin1String("umlaut") + QChar(0xEB);
}

static void removeFile(const QString &trashDir, const QString &fileName)
{
    QDir dir;
    dir.remove(trashDir + fileName);
    QVERIFY(!QDir(trashDir + fileName).exists());
}

static void removeDir(const QString &trashDir, const QString &dirName)
{
    QDir dir;
    dir.rmdir(trashDir + dirName);
    QVERIFY(!QDir(trashDir + dirName).exists());
}

static void removeDirRecursive(const QString &dir)
{
    if (QFile::exists(dir)) {

        // Make it work even with readonly dirs, like trashReadOnlyDirFromHome() creates
        QUrl u = QUrl::fromLocalFile(dir);
        //qDebug() << "chmod +0200 on" << u;
        KFileItem fileItem(u, QStringLiteral("inode/directory"), KFileItem::Unknown);
        KFileItemList fileItemList;
        fileItemList.append(fileItem);
        KIO::ChmodJob *chmodJob = KIO::chmod(fileItemList, 0200, 0200, QString(), QString(), true /*recursive*/, KIO::HideProgressInfo);
        chmodJob->exec();

        KIO::Job *delJob = KIO::del(u, KIO::HideProgressInfo);
        if (!delJob->exec()) {
            qFatal("Couldn't delete %s", qPrintable(dir));
        }
    }
}

void TestTrash::initTestCase()
{
    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");

    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(m_tempDir.isValid());

#ifndef Q_OS_OSX
    m_trashDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
    qDebug() << "setup: using trash directory " << m_trashDir;
#endif

    // Look for another writable partition than $HOME (not mandatory)
    TrashImpl impl;
    impl.init();

    TrashImpl::TrashDirMap trashDirs = impl.trashDirectories();
#ifdef Q_OS_OSX
    QVERIFY(trashDirs.contains(0));
    m_trashDir = trashDirs.value(0);
    qDebug() << "setup: using trash directory " << m_trashDir;
#endif

    TrashImpl::TrashDirMap topDirs = impl.topDirectories();
    bool foundTrashDir = false;
    m_otherPartitionId = 0;
    m_tmpIsWritablePartition = false;
    m_tmpTrashId = -1;
    QVector<int> writableTopDirs;
    for (TrashImpl::TrashDirMap::ConstIterator it = trashDirs.constBegin(); it != trashDirs.constEnd(); ++it) {
        if (it.key() == 0) {
            QCOMPARE(it.value(), m_trashDir);
            QVERIFY(topDirs.find(0) == topDirs.end());
            foundTrashDir = true;
        } else {
            QVERIFY(topDirs.find(it.key()) != topDirs.end());
            const QString topdir = topDirs[it.key()];
            if (QFileInfo(topdir).isWritable()) {
                writableTopDirs.append(it.key());
                if (topdir == QLatin1String("/tmp/")) {
                    m_tmpIsWritablePartition = true;
                    m_tmpTrashId = it.key();
                    qDebug() << "/tmp is on its own partition (trashid=" << m_tmpTrashId << "), some tests will be skipped";
                    removeFile(it.value(), QStringLiteral("/info/fileFromOther.trashinfo"));
                    removeFile(it.value(), QStringLiteral("/files/fileFromOther"));
                    removeFile(it.value(), QStringLiteral("/info/symlinkFromOther.trashinfo"));
                    removeFile(it.value(), QStringLiteral("/files/symlinkFromOther"));
                    removeFile(it.value(), QStringLiteral("/info/trashDirFromOther.trashinfo"));
                    removeFile(it.value(), QStringLiteral("/files/trashDirFromOther/testfile"));
                    removeDir(it.value(), QStringLiteral("/files/trashDirFromOther"));
                }
            }
        }
    }
    for (QVector<int>::const_iterator it = writableTopDirs.constBegin(); it != writableTopDirs.constEnd(); ++it) {
        const QString topdir = topDirs[ *it ];
        const QString trashdir = trashDirs[ *it ];
        QVERIFY(!topdir.isEmpty());
        QVERIFY(!trashDirs.isEmpty());
        if (topdir != QLatin1String("/tmp/") ||         // we'd prefer not to use /tmp here, to separate the tests
                (writableTopDirs.count() > 1)) { // but well, if we have no choice, take it
            m_otherPartitionTopDir = topdir;
            m_otherPartitionTrashDir = trashdir;
            m_otherPartitionId = *it;
            qDebug() << "OK, found another writable partition: topDir=" << m_otherPartitionTopDir
                     << " trashDir=" << m_otherPartitionTrashDir << " id=" << m_otherPartitionId;
            break;
        }
    }
    // Check that m_trashDir got listed
    QVERIFY(foundTrashDir);
    if (m_otherPartitionTrashDir.isEmpty()) {
        qWarning() << "No writable partition other than $HOME found, some tests will be skipped";
    }

    // Start with a clean base dir
    qDebug() << "initial cleanup";
    removeDirRecursive(homeTmpDir());

    QDir dir; // TT: why not a static method?
    bool ok = dir.mkdir(homeTmpDir());
    if (!ok) {
        qFatal("Couldn't create directory: %s", qPrintable(homeTmpDir()));
    }
    QVERIFY(QFileInfo(otherTmpDir()).isDir());

    // Start with a clean trash too
    qDebug() << "removing trash dir";
    removeDirRecursive(m_trashDir);
}

void TestTrash::cleanupTestCase()
{
    // Clean up
    removeDirRecursive(homeTmpDir());
    removeDirRecursive(otherTmpDir());
    removeDirRecursive(m_trashDir);
}

void TestTrash::urlTestFile()
{
    const QUrl url = TrashImpl::makeURL(1, QStringLiteral("fileId"), QString());
    QCOMPARE(url.url(), QStringLiteral("trash:/1-fileId"));

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    QVERIFY(ok);
    QCOMPARE(QString::number(trashId), QStringLiteral("1"));
    QCOMPARE(fileId, QStringLiteral("fileId"));
    QCOMPARE(relativePath, QString());
}

void TestTrash::urlTestDirectory()
{
    const QUrl url = TrashImpl::makeURL(1, QStringLiteral("fileId"), QStringLiteral("subfile"));
    QCOMPARE(url.url(), QStringLiteral("trash:/1-fileId/subfile"));

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    QVERIFY(ok);
    QCOMPARE(trashId, 1);
    QCOMPARE(fileId, QStringLiteral("fileId"));
    QCOMPARE(relativePath, QStringLiteral("subfile"));
}

void TestTrash::urlTestSubDirectory()
{
    const QUrl url = TrashImpl::makeURL(1, QStringLiteral("fileId"), QStringLiteral("subfile/foobar"));
    QCOMPARE(url.url(), QStringLiteral("trash:/1-fileId/subfile/foobar"));

    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    QVERIFY(ok);
    QCOMPARE(trashId, 1);
    QCOMPARE(fileId, QStringLiteral("fileId"));
    QCOMPARE(relativePath, QStringLiteral("subfile/foobar"));
}

static void checkInfoFile(const QString &infoPath, const QString &origFilePath)
{
    qDebug() << infoPath;
    QFileInfo info(infoPath);
    QVERIFY2(info.exists(), qPrintable(infoPath));
    QVERIFY(info.isFile());
    KConfig infoFile(info.absoluteFilePath());
    KConfigGroup group = infoFile.group("Trash Info");
    if (!group.exists()) {
        qFatal("no Trash Info group in %s", qPrintable(info.absoluteFilePath()));
    }
    const QString origPath = group.readEntry("Path");
    QVERIFY(!origPath.isEmpty());
    QCOMPARE(origPath.toUtf8(), QUrl::toPercentEncoding(origFilePath, "/"));
    if (origFilePath.contains(QChar(0x2153)) || origFilePath.contains(QLatin1Char('%')) || origFilePath.contains(QLatin1String("umlaut"))) {
        QVERIFY(origPath.contains(QLatin1Char('%')));
    } else {
        QVERIFY(!origPath.contains(QLatin1Char('%')));
    }
    const QString date = group.readEntry("DeletionDate");
    QVERIFY(!date.isEmpty());
    QVERIFY(date.contains(QLatin1String("T")));
}

static void createTestFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        qFatal("Can't create %s", qPrintable(path));
    }
    f.write("Hello world\n", 12);
    f.close();
    QVERIFY(QFile::exists(path));
}

void TestTrash::trashFile(const QString &origFilePath, const QString &fileId)
{
    // setup
    if (!QFile::exists(origFilePath)) {
        createTestFile(origFilePath);
    }
    QUrl u = QUrl::fromLocalFile(origFilePath);

    // test
    KIO::Job *job = KIO::move(u, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    bool ok = job->exec();
    if (!ok) {
        qCritical() << "moving " << u << " to trash failed with error " << job->error() << " " << job->errorString();
    }
    QVERIFY(ok);
    if (origFilePath.startsWith(QLatin1String("/tmp")) && m_tmpIsWritablePartition) {
        qDebug() << " TESTS SKIPPED";
    } else {
        checkInfoFile(m_trashDir + QLatin1String("/info/") + fileId + QLatin1String(".trashinfo"), origFilePath);

        QFileInfo files(m_trashDir + QLatin1String("/files/") + fileId);
        QVERIFY(files.isFile());
        QCOMPARE(files.size(), 12);
    }

    // coolo suggests testing that the original file is actually gone, too :)
    QVERIFY(!QFile::exists(origFilePath));

    QMap<QString, QString> metaData = job->metaData();
    QVERIFY(!metaData.isEmpty());
    bool found = false;
    QMap<QString, QString>::ConstIterator it = metaData.constBegin();
    for (; it != metaData.constEnd(); ++it) {
        if (it.key().startsWith(QLatin1String("trashURL"))) {
            QUrl trashURL(it.value());
            qDebug() << trashURL;
            QVERIFY(!trashURL.isEmpty());
            QCOMPARE(trashURL.scheme(), QLatin1String("trash"));
            int trashId = 0;
            if (origFilePath.startsWith(QLatin1String("/tmp")) && m_tmpIsWritablePartition) {
                trashId = m_tmpTrashId;
            }
            QCOMPARE(trashURL.path(), QString(QStringLiteral("/") + QString::number(trashId) + QLatin1Char('-') + fileId));
            found = true;
        }
    }
    QVERIFY(found);
}

void TestTrash::trashFileFromHome()
{
    const QString fileName = QStringLiteral("fileFromHome");
    trashFile(homeTmpDir() + fileName, fileName);

    // Do it again, check that we got a different id
    trashFile(homeTmpDir() + fileName, fileName + QLatin1String(" (1)"));
}

void TestTrash::trashPercentFileFromHome()
{
    const QString fileName = QStringLiteral("file%2f");
    trashFile(homeTmpDir() + fileName, fileName);
}

void TestTrash::trashUtf8FileFromHome()
{
#ifdef UTF8TEST
    const QString fileName = utf8FileName();
    trashFile(homeTmpDir() + fileName, fileName);
#endif
}

void TestTrash::trashUmlautFileFromHome()
{
    const QString fileName = umlautFileName();
    trashFile(homeTmpDir() + fileName, fileName);
}

void TestTrash::testTrashNotEmpty()
{
    KConfig cfg(QStringLiteral("trashrc"), KConfig::SimpleConfig);
    const KConfigGroup group = cfg.group("Status");
    QVERIFY(group.exists());
    QCOMPARE(group.readEntry("Empty", true), false);
}

void TestTrash::trashFileFromOther()
{
    const QString fileName = QStringLiteral("fileFromOther");
    trashFile(otherTmpDir() + fileName, fileName);
}

void TestTrash::trashFileIntoOtherPartition()
{
    if (m_otherPartitionTrashDir.isEmpty()) {
        qDebug() << " - SKIPPED";
        return;
    }
    const QString fileName = QStringLiteral("testtrash-file");
    const QString origFilePath = m_otherPartitionTopDir + fileName;
    const QString &fileId = fileName;
    // cleanup
    QFile::remove(m_otherPartitionTrashDir + QLatin1String("/info/") + fileId + QLatin1String(".trashinfo"));
    QFile::remove(m_otherPartitionTrashDir + QLatin1String("/files/") + fileId);

    // setup
    if (!QFile::exists(origFilePath)) {
        createTestFile(origFilePath);
    }
    QUrl u = QUrl::fromLocalFile(origFilePath);

    // test
    KIO::Job *job = KIO::move(u, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(ok);
    QMap<QString, QString> metaData = job->metaData();
    // Note that the Path stored in the info file is relative, on other partitions (#95652)
    checkInfoFile(m_otherPartitionTrashDir + QLatin1String("/info/") + fileId + QLatin1String(".trashinfo"), fileName);

    QFileInfo files(m_otherPartitionTrashDir + QLatin1String("/files/") + fileId);
    QVERIFY(files.isFile());
    QCOMPARE(files.size(), 12);

    // coolo suggests testing that the original file is actually gone, too :)
    QVERIFY(!QFile::exists(origFilePath));

    QVERIFY(!metaData.isEmpty());
    bool found = false;
    QMap<QString, QString>::ConstIterator it = metaData.constBegin();
    for (; it != metaData.constEnd(); ++it) {
        if (it.key().startsWith(QLatin1String("trashURL"))) {
            QUrl trashURL(it.value());
            qDebug() << trashURL;
            QVERIFY(!trashURL.isEmpty());
            QCOMPARE(trashURL.scheme(), QLatin1String("trash"));
            QCOMPARE(trashURL.path(), QStringLiteral("/%1-%2").arg(m_otherPartitionId).arg(fileId));
            found = true;
        }
    }
    QVERIFY(found);
}

void TestTrash::trashFileOwnedByRoot()
{
    QUrl u(QStringLiteral("file:///etc/passwd"));
    const QString fileId = QStringLiteral("passwd");

    if (geteuid() == 0 || QFileInfo(u.toLocalFile()).isWritable()) {
        QSKIP("Test must not be run by root.");
    }

    KIO::CopyJob *job = KIO::move(u, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr); // no skip dialog, thanks
    bool ok = job->exec();
    QVERIFY(!ok);

    QCOMPARE(job->error(), KIO::ERR_ACCESS_DENIED);
    const QString infoPath(m_trashDir + QLatin1String("/info/") + fileId + QLatin1String(".trashinfo"));
    QVERIFY(!QFile::exists(infoPath));

    QFileInfo files(m_trashDir + QLatin1String("/files/") + fileId);
    QVERIFY(!files.exists());

    QVERIFY(QFile::exists(u.path()));
}

void TestTrash::trashSymlink(const QString &origFilePath, const QString &fileId, bool broken)
{
    // setup
    const char *target = broken ? "/nonexistent" : "/tmp";
    bool ok = ::symlink(target, QFile::encodeName(origFilePath).constData()) == 0;
    QVERIFY(ok);
    QUrl u = QUrl::fromLocalFile(origFilePath);

    // test
    KIO::Job *job = KIO::move(u, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    ok = job->exec();
    QVERIFY(ok);
    if (origFilePath.startsWith(QLatin1String("/tmp")) && m_tmpIsWritablePartition) {
        qDebug() << " TESTS SKIPPED";
        return;
    }
    checkInfoFile(m_trashDir + QLatin1String("/info/") + fileId + QLatin1String(".trashinfo"), origFilePath);

    QFileInfo files(m_trashDir + QLatin1String("/files/") + fileId);
    QVERIFY(files.isSymLink());
    QCOMPARE(files.symLinkTarget(), QFile::decodeName(target));
    QVERIFY(!QFile::exists(origFilePath));
}

void TestTrash::trashSymlinkFromHome()
{
    const QString fileName = QStringLiteral("symlinkFromHome");
    trashSymlink(homeTmpDir() + fileName, fileName, false);
}

void TestTrash::trashSymlinkFromOther()
{
    const QString fileName = QStringLiteral("symlinkFromOther");
    trashSymlink(otherTmpDir() + fileName, fileName, false);
}

void TestTrash::trashBrokenSymlinkFromHome()
{
    const QString fileName = QStringLiteral("brokenSymlinkFromHome");
    trashSymlink(homeTmpDir() + fileName, fileName, true);
}

void TestTrash::trashDirectory(const QString &origPath, const QString &fileId)
{
    qDebug() << fileId;
    // setup
    if (!QFileInfo::exists(origPath)) {
        QDir dir;
        bool ok = dir.mkdir(origPath);
        QVERIFY(ok);
    }
    createTestFile(origPath + QLatin1String("/testfile"));
    QVERIFY(QDir().mkdir(origPath + QStringLiteral("/subdir")));
    createTestFile(origPath + QLatin1String("/subdir/subfile"));
    QUrl u = QUrl::fromLocalFile(origPath);

    // test
    KIO::Job *job = KIO::move(u, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    QVERIFY(job->exec());
    if (origPath.startsWith(QLatin1String("/tmp")) && m_tmpIsWritablePartition) {
        qDebug() << " TESTS SKIPPED";
        return;
    }
    checkInfoFile(m_trashDir + QLatin1String("/info/") + fileId + QLatin1String(".trashinfo"), origPath);

    QFileInfo filesDir(m_trashDir + QLatin1String("/files/") + fileId);
    QVERIFY(filesDir.isDir());
    QFileInfo files(m_trashDir + QLatin1String("/files/") + fileId + QLatin1String("/testfile"));
    QVERIFY(files.exists());
    QVERIFY(files.isFile());
    QCOMPARE(files.size(), 12);
    QVERIFY(!QFile::exists(origPath));
    QVERIFY(QFile::exists(m_trashDir + QStringLiteral("/files/") + fileId + QStringLiteral("/subdir/subfile")));

    QFile dirCache(m_trashDir + QLatin1String("/directorysizes"));
    QVERIFY2(dirCache.open(QIODevice::ReadOnly), qPrintable(dirCache.fileName()));
    QByteArray lines;
    bool found = false;
    while (!dirCache.atEnd()) {
        const QByteArray line = dirCache.readLine();
        if (line.endsWith(' ' + QFile::encodeName(fileId).toPercentEncoding() + '\n')) {
            QVERIFY(!found); // should be there only once!
            found = true;
        }
        lines += line;
    }
    QVERIFY2(found, lines.constData());
    //qDebug() << lines;

    checkDirCacheValidity();
}

void TestTrash::checkDirCacheValidity()
{
    QFile dirCache(m_trashDir + QLatin1String("/directorysizes"));
    QVERIFY2(dirCache.open(QIODevice::ReadOnly), qPrintable(dirCache.fileName()));
    QSet<QByteArray> seenDirs;
    while (!dirCache.atEnd()) {
        QByteArray line = dirCache.readLine();
        QVERIFY(line.endsWith('\n'));
        line.chop(1);
        qDebug() << "LINE" << line;

        const auto exploded = line.split(' ');
        QCOMPARE(exploded.size(), 3);

        bool succeeded = false;
        const int size = exploded.at(0).toInt(&succeeded);
        QVERIFY(succeeded);
        QVERIFY(size > 0);

        const int mtime = exploded.at(0).toInt(&succeeded);
        QVERIFY(succeeded);
        QVERIFY(mtime > 0);
        QVERIFY(QDateTime::fromMSecsSinceEpoch(mtime).isValid());

        const QByteArray dir = QByteArray::fromPercentEncoding(exploded.at(2));
        QVERIFY2(!seenDirs.contains(dir), dir.constData());
        seenDirs.insert(dir);
        const QString localDir = m_trashDir + QLatin1String("/files/") + QFile::decodeName(dir);
        QVERIFY2(QFile::exists(localDir), qPrintable(localDir));
        QVERIFY(QFileInfo(localDir).isDir());
    }
}

void TestTrash::trashDirectoryFromHome()
{
    QString dirName = QStringLiteral("trashDirFromHome");
    trashDirectory(homeTmpDir() + dirName, dirName);
    checkDirCacheValidity();
    // Do it again, check that we got a different id
    trashDirectory(homeTmpDir() + dirName, dirName + QLatin1String(" (1)"));
}

void TestTrash::trashDotDirectory()
{
    QString dirName = QStringLiteral(".dotTrashDirFromHome");
    trashDirectory(homeTmpDir() + dirName, dirName);
    // Do it again, check that we got a different id
    // TODO trashDirectory(homeTmpDir() + dirName, dirName + QString::fromLatin1(" (1)"));
}

void TestTrash::trashReadOnlyDirFromHome()
{
    const QString dirName = readOnlyDirPath();
    QDir dir;
    bool ok = dir.mkdir(dirName);
    QVERIFY(ok);
    // #130780
    const QString subDirPath = dirName + QLatin1String("/readonly_subdir");
    ok = dir.mkdir(subDirPath);
    QVERIFY(ok);
    createTestFile(subDirPath + QLatin1String("/testfile_in_subdir"));
    ::chmod(QFile::encodeName(subDirPath).constData(), 0500);

    trashDirectory(dirName, QStringLiteral("readonly"));
}

void TestTrash::trashDirectoryFromOther()
{
    QString dirName = QStringLiteral("trashDirFromOther");
    trashDirectory(otherTmpDir() + dirName, dirName);
}

void TestTrash::trashDirectoryWithTrailingSlash()
{
    QString dirName = QStringLiteral("dirwithslash/");
    trashDirectory(homeTmpDir() + dirName, QStringLiteral("dirwithslash"));
}

void TestTrash::trashBrokenSymlinkIntoSubdir()
{
    QString origPath = homeTmpDir() + QStringLiteral("subDirBrokenSymlink");

    if (!QFileInfo::exists(origPath)) {
        QDir dir;
        bool ok = dir.mkdir(origPath);
        QVERIFY(ok);
    }
    bool ok = ::symlink("/nonexistent", QFile::encodeName(origPath + QStringLiteral("/link")).constData()) == 0;
    QVERIFY(ok);

    trashDirectory(origPath, QStringLiteral("subDirBrokenSymlink"));
}

void TestTrash::testRemoveStaleInfofile()
{
    const QString fileName = QStringLiteral("disappearingFileInTrash");
    const QString filePath = homeTmpDir() + fileName;
    createTestFile(filePath);
    trashFile(filePath, fileName);

    const QString pathInTrash = m_trashDir + QLatin1String("/files/") + QLatin1String("disappearingFileInTrash");
    // remove the file without using KIO
    QVERIFY(QFile::remove(pathInTrash));

    // .trashinfo file still exists
    const QString infoPath = m_trashDir + QLatin1String("/info/disappearingFileInTrash.trashinfo");
    QVERIFY(QFile(infoPath).exists());

    KIO::ListJob *job = KIO::listDir(QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    connect(job, &KIO::ListJob::entries, this, &TestTrash::slotEntries);
    QVERIFY(job->exec());

    // during the list job, kio_trash should have deleted the .trashinfo file since it
    // references a trashed file that doesn't exist any more
    QVERIFY(!QFile(infoPath).exists());
}

void TestTrash::delRootFile()
{
    // test deleting a trashed file
    KIO::Job *delJob = KIO::del(QUrl(QStringLiteral("trash:/0-fileFromHome")), KIO::HideProgressInfo);
    bool ok = delJob->exec();
    QVERIFY(ok);

    QFileInfo file(m_trashDir + QLatin1String("/files/fileFromHome"));
    QVERIFY(!file.exists());
    QFileInfo info(m_trashDir + QLatin1String("/info/fileFromHome.trashinfo"));
    QVERIFY(!info.exists());

    // trash it again, we might need it later
    const QString fileName = QStringLiteral("fileFromHome");
    trashFile(homeTmpDir() + fileName, fileName);
}

void TestTrash::delFileInDirectory()
{
    // test deleting a file inside a trashed directory -> not allowed
    KIO::Job *delJob = KIO::del(QUrl(QStringLiteral("trash:/0-trashDirFromHome/testfile")), KIO::HideProgressInfo);
    bool ok = delJob->exec();
    QVERIFY(!ok);
    QCOMPARE(delJob->error(), KIO::ERR_ACCESS_DENIED);

    QFileInfo dir(m_trashDir + QLatin1String("/files/trashDirFromHome"));
    QVERIFY(dir.exists());
    QFileInfo file(m_trashDir + QLatin1String("/files/trashDirFromHome/testfile"));
    QVERIFY(file.exists());
    QFileInfo info(m_trashDir + QLatin1String("/info/trashDirFromHome.trashinfo"));
    QVERIFY(info.exists());
}

void TestTrash::delDirectory()
{
    // test deleting a trashed directory
    KIO::Job *delJob = KIO::del(QUrl(QStringLiteral("trash:/0-trashDirFromHome")), KIO::HideProgressInfo);
    bool ok = delJob->exec();
    QVERIFY(ok);

    QFileInfo dir(m_trashDir + QLatin1String("/files/trashDirFromHome"));
    QVERIFY(!dir.exists());
    QFileInfo file(m_trashDir + QLatin1String("/files/trashDirFromHome/testfile"));
    QVERIFY(!file.exists());
    QFileInfo info(m_trashDir + QLatin1String("/info/trashDirFromHome.trashinfo"));
    QVERIFY(!info.exists());

    checkDirCacheValidity();

    // trash it again, we'll need it later
    QString dirName = QStringLiteral("trashDirFromHome");
    trashDirectory(homeTmpDir() + dirName, dirName);
}

static bool MyNetAccess_stat(const QUrl &url, KIO::UDSEntry &entry)
{
    KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
    bool ok = statJob->exec();
    if (ok) {
        entry = statJob->statResult();
    }
    return ok;
}
static bool MyNetAccess_exists(const QUrl &url)
{
    KIO::UDSEntry dummy;
    return MyNetAccess_stat(url, dummy);
}

void TestTrash::mostLocalUrlTest()
{
    const QStringList trashFiles = QDir(m_trashDir + QLatin1String("/files/")).entryList();
    for (const QString &file : trashFiles) {
        if (file == QLatin1Char('.') || file == QLatin1String("..")) {
            continue;
        }
        QUrl url;
        url.setScheme(QStringLiteral("trash"));
        url.setPath(QLatin1String("0-") + file);
        KIO::StatJob *statJob = KIO::mostLocalUrl(url, KIO::HideProgressInfo);
        QVERIFY(statJob->exec());
        QCOMPARE(url, statJob->mostLocalUrl());
    }
}

void TestTrash::statRoot()
{
    QUrl url(QStringLiteral("trash:/"));
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat(url, entry);
    QVERIFY(ok);
    KFileItem item(entry, url);
    QVERIFY(item.isDir());
    QVERIFY(!item.isLink());
    QVERIFY(item.isReadable());
    QVERIFY(item.isWritable());
    QVERIFY(!item.isHidden());
    QCOMPARE(item.name(), QStringLiteral("."));
}

void TestTrash::statFileInRoot()
{
    QUrl url(QStringLiteral("trash:/0-fileFromHome"));
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat(url, entry);
    QVERIFY(ok);
    KFileItem item(entry, url);
    QVERIFY(item.isFile());
    QVERIFY(!item.isDir());
    QVERIFY(!item.isLink());
    QVERIFY(item.isReadable());
    QVERIFY(!item.isWritable());
    QVERIFY(!item.isHidden());
    QCOMPARE(item.text(), QStringLiteral("fileFromHome"));
}

void TestTrash::statDirectoryInRoot()
{
    QUrl url(QStringLiteral("trash:/0-trashDirFromHome"));
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat(url, entry);
    QVERIFY(ok);
    KFileItem item(entry, url);
    QVERIFY(item.isDir());
    QVERIFY(!item.isLink());
    QVERIFY(item.isReadable());
    QVERIFY(!item.isWritable());
    QVERIFY(!item.isHidden());
    QCOMPARE(item.text(), QStringLiteral("trashDirFromHome"));
}

void TestTrash::statSymlinkInRoot()
{
    QUrl url(QStringLiteral("trash:/0-symlinkFromHome"));
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat(url, entry);
    QVERIFY(ok);
    KFileItem item(entry, url);
    QVERIFY(item.isLink());
    QCOMPARE(item.linkDest(), QStringLiteral("/tmp"));
    QVERIFY(item.isReadable());
    QVERIFY(!item.isWritable());
    QVERIFY(!item.isHidden());
    QCOMPARE(item.text(), QStringLiteral("symlinkFromHome"));
}

void TestTrash::statFileInDirectory()
{
    QUrl url(QStringLiteral("trash:/0-trashDirFromHome/testfile"));
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat(url, entry);
    QVERIFY(ok);
    KFileItem item(entry, url);
    QVERIFY(item.isFile());
    QVERIFY(!item.isLink());
    QVERIFY(item.isReadable());
    QVERIFY(!item.isWritable());
    QVERIFY(!item.isHidden());
    QCOMPARE(item.text(), QStringLiteral("testfile"));
}

void TestTrash::statBrokenSymlinkInSubdir()
{
    QUrl url(QStringLiteral("trash:/0-subDirBrokenSymlink/link"));
    KIO::UDSEntry entry;
    bool ok = MyNetAccess_stat(url, entry);
    QVERIFY(ok);
    KFileItem item(entry, url);
    QVERIFY(item.isLink());
    QVERIFY(item.isReadable());
    QVERIFY(!item.isWritable());
    QVERIFY(!item.isHidden());
    QCOMPARE(item.linkDest(), QLatin1String("/nonexistent"));
}

void TestTrash::copyFromTrash(const QString &fileId, const QString &destPath, const QString &relativePath)
{
    QUrl src(QLatin1String("trash:/0-") + fileId);
    if (!relativePath.isEmpty()) {
        src.setPath(concatPaths(src.path(), relativePath));
    }
    QUrl dest = QUrl::fromLocalFile(destPath);

    QVERIFY(MyNetAccess_exists(src));

    // A dnd would use copy(), but we use copyAs to ensure the final filename
    //qDebug() << "copyAs:" << src << " -> " << dest;
    KIO::Job *job = KIO::copyAs(src, dest, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY2(ok, qPrintable(job->errorString()));
    QString infoFile(m_trashDir + QLatin1String("/info/") + fileId + QLatin1String(".trashinfo"));
    QVERIFY(QFile::exists(infoFile));

    QFileInfo filesItem(m_trashDir + QLatin1String("/files/") + fileId);
    QVERIFY(filesItem.exists());

    QVERIFY(QFile::exists(destPath));
}

void TestTrash::copyFileFromTrash()
{
// To test case of already-existing destination, uncomment this.
// This brings up the "rename" dialog though, so it can't be fully automated
#if 0
    const QString destPath = otherTmpDir() + QString::fromLatin1("fileFromHome_copied");
    copyFromTrash("fileFromHome", destPath);
    QVERIFY(QFileInfo(destPath).isFile());
    QCOMPARE(QFileInfo(destPath).size(), 12);
#endif
}

void TestTrash::copyFileInDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + QLatin1String("testfile_copied");
    copyFromTrash(QStringLiteral("trashDirFromHome"), destPath, QStringLiteral("testfile"));
    QVERIFY(QFileInfo(destPath).isFile());
    QCOMPARE(QFileInfo(destPath).size(), 12);
    QVERIFY(QFileInfo(destPath).isWritable());
}

void TestTrash::copyDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + QLatin1String("trashDirFromHome_copied");
    copyFromTrash(QStringLiteral("trashDirFromHome"), destPath);
    QVERIFY(QFileInfo(destPath).isDir());
    QVERIFY(QFile::exists(destPath + QStringLiteral("/testfile")));
    QVERIFY(QFile::exists(destPath + QStringLiteral("/subdir/subfile")));
}

void TestTrash::copySymlinkFromTrash() // relies on trashSymlinkFromHome() being called first
{
    const QString destPath = otherTmpDir() + QLatin1String("symlinkFromHome_copied");
    copyFromTrash(QStringLiteral("symlinkFromHome"), destPath);
    QVERIFY(QFileInfo(destPath).isSymLink());
}

void TestTrash::moveInTrash(const QString &fileId, const QString &destFileId)
{
    const QUrl src(QLatin1String("trash:/0-") + fileId);
    const QUrl dest(QLatin1String("trash:/") + destFileId);

    QVERIFY(MyNetAccess_exists(src));
    QVERIFY(!MyNetAccess_exists(dest));

    KIO::Job *job = KIO::moveAs(src, dest, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY2(ok, qPrintable(job->errorString()));

    // Check old doesn't exist anymore
    QString infoFile(m_trashDir + QStringLiteral("/info/") + fileId + QStringLiteral(".trashinfo"));
    QVERIFY(!QFile::exists(infoFile));
    QFileInfo filesItem(m_trashDir + QStringLiteral("/files/") + fileId);
    QVERIFY(!filesItem.exists());

    // Check new exists now
    QString newInfoFile(m_trashDir + QStringLiteral("/info/") + destFileId + QStringLiteral(".trashinfo"));
    QVERIFY(QFile::exists(newInfoFile));
    QFileInfo newFilesItem(m_trashDir + QStringLiteral("/files/") + destFileId);
    QVERIFY(newFilesItem.exists());

}

void TestTrash::renameFileInTrash()
{
    const QString fileName = QStringLiteral("renameFileInTrash");
    const QString filePath = homeTmpDir() + fileName;
    createTestFile(filePath);
    trashFile(filePath, fileName);

    const QString destFileName = QStringLiteral("fileRenamed");
    moveInTrash(fileName, destFileName);

    // cleanup
    KIO::Job *delJob = KIO::del(QUrl(QStringLiteral("trash:/0-fileRenamed")), KIO::HideProgressInfo);
    bool ok = delJob->exec();
    QVERIFY2(ok, qPrintable(delJob->errorString()));
}

void TestTrash::renameDirInTrash()
{
    const QString dirName = QStringLiteral("trashDirFromHome");
    const QString destDirName = QStringLiteral("dirRenamed");
    moveInTrash(dirName, destDirName);
    moveInTrash(destDirName, dirName);
}

void TestTrash::moveFromTrash(const QString &fileId, const QString &destPath, const QString &relativePath)
{
    QUrl src(QLatin1String("trash:/0-") + fileId);
    if (!relativePath.isEmpty()) {
        src.setPath(concatPaths(src.path(), relativePath));
    }
    QUrl dest = QUrl::fromLocalFile(destPath);

    QVERIFY(MyNetAccess_exists(src));

    // A dnd would use move(), but we use moveAs to ensure the final filename
    KIO::Job *job = KIO::moveAs(src, dest, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY2(ok, qPrintable(job->errorString()));
    QString infoFile(m_trashDir + QStringLiteral("/info/") + fileId + QStringLiteral(".trashinfo"));
    QVERIFY(!QFile::exists(infoFile));

    QFileInfo filesItem(m_trashDir + QStringLiteral("/files/") + fileId);
    QVERIFY(!filesItem.exists());

    QVERIFY(QFile::exists(destPath));
    QVERIFY(QFileInfo(destPath).isWritable());
}

void TestTrash::moveFileFromTrash()
{
    const QString fileName = QStringLiteral("moveFileFromTrash");
    const QString filePath = homeTmpDir() + fileName;
    createTestFile(filePath);
    const QFile::Permissions origPerms = QFileInfo(filePath).permissions();
    trashFile(filePath, fileName);

    const QString destPath = otherTmpDir() + QStringLiteral("fileFromTrash_restored");
    moveFromTrash(fileName, destPath);
    const QFileInfo destInfo(destPath);
    QVERIFY(destInfo.isFile());
    QCOMPARE(destInfo.size(), 12);
    QVERIFY(destInfo.isWritable());
    QCOMPARE(int(destInfo.permissions()), int(origPerms));

    QVERIFY(QFile::remove(destPath));
}

void TestTrash::moveFileFromTrashToDir_data()
{
    QTest::addColumn<QString>("destDir");

    QTest::newRow("home_partition") << homeTmpDir(); // this will trigger a direct renaming
    QTest::newRow("other_partition") << otherTmpDir(); // this will require a real move

}

void TestTrash::moveFileFromTrashToDir()
{
    // Given a file in the trash
    const QString fileName = QStringLiteral("moveFileFromTrashToDir");
    const QString filePath = homeTmpDir() + fileName;
    createTestFile(filePath);
    const QFile::Permissions origPerms = QFileInfo(filePath).permissions();
    trashFile(filePath, fileName);
    QVERIFY(!QFile::exists(filePath));

    // When moving it out to a dir
    QFETCH(QString, destDir);
    const QString destPath = destDir + QStringLiteral("moveFileFromTrashToDir");
    const QUrl src(QLatin1String("trash:/0-") + fileName);
    const QUrl dest(QUrl::fromLocalFile(destDir));
    KIO::Job *job = KIO::move(src, dest, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY2(ok, qPrintable(job->errorString()));

    // Then it should move ;)
    const QFileInfo destInfo(destPath);
    QVERIFY(destInfo.isFile());
    QCOMPARE(destInfo.size(), 12);
    QVERIFY(destInfo.isWritable());
    QCOMPARE(int(destInfo.permissions()), int(origPerms));

    QVERIFY(QFile::remove(destPath));
}

void TestTrash::moveFileInDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + QStringLiteral("testfile_restored");
    copyFromTrash(QStringLiteral("trashDirFromHome"), destPath, QStringLiteral("testfile"));
    QVERIFY(QFileInfo(destPath).isFile());
    QCOMPARE(QFileInfo(destPath).size(), 12);
}

void TestTrash::moveDirectoryFromTrash()
{
    const QString destPath = otherTmpDir() + QStringLiteral("trashDirFromHome_restored");
    moveFromTrash(QStringLiteral("trashDirFromHome"), destPath);
    QVERIFY(QFileInfo(destPath).isDir());
    checkDirCacheValidity();

    // trash it again, we'll need it later
    QString dirName = QStringLiteral("trashDirFromHome");
    trashDirectory(homeTmpDir() + dirName, dirName);
}

void TestTrash::trashDirectoryOwnedByRoot()
{
    QUrl u(QStringLiteral("file:///"));;
    if (QFile::exists(QStringLiteral("/etc/cups"))) {
        u.setPath(QStringLiteral("/etc/cups"));
    } else if (QFile::exists(QStringLiteral("/boot"))) {
        u.setPath(QStringLiteral("/boot"));
    } else {
        u.setPath(QStringLiteral("/etc"));
    }
    const QString fileId = u.path();
    qDebug() << "fileId=" << fileId;

    if (geteuid() == 0 || QFileInfo(u.toLocalFile()).isWritable()) {
        QSKIP("Test must not be run by root.");
    }

    KIO::CopyJob *job = KIO::move(u, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr); // no skip dialog, thanks
    bool ok = job->exec();
    QVERIFY(!ok);
    const int err = job->error();
    QVERIFY(err == KIO::ERR_ACCESS_DENIED
            || err == KIO::ERR_CANNOT_OPEN_FOR_READING);

    const QString infoPath(m_trashDir + QStringLiteral("/info/") + fileId + QStringLiteral(".trashinfo"));
    QVERIFY(!QFile::exists(infoPath));

    QFileInfo files(m_trashDir + QStringLiteral("/files/") + fileId);
    QVERIFY(!files.exists());

    QVERIFY(QFile::exists(u.path()));
}

void TestTrash::moveSymlinkFromTrash()
{
    const QString destPath = otherTmpDir() + QStringLiteral("symlinkFromHome_restored");
    moveFromTrash(QStringLiteral("symlinkFromHome"), destPath);
    QVERIFY(QFileInfo(destPath).isSymLink());
}

void TestTrash::testMoveNonExistingFile()
{
    const QUrl dest = QUrl::fromLocalFile(homeTmpDir() + QLatin1String("DoesNotExist"));
    KIO::Job *job =
        KIO::file_move(QUrl(QStringLiteral("trash:/0-DoesNotExist")), dest, -1, KIO::HideProgressInfo);

    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_DOES_NOT_EXIST);
    QCOMPARE(job->errorString(), QStringLiteral("The file or folder trash:/DoesNotExist does not exist."));

}

void TestTrash::getFile()
{
    const QString fileId = QStringLiteral("fileFromHome (1)");
    const QUrl url = TrashImpl::makeURL(0, fileId, QString());

    QTemporaryFile tmpFile;
    QVERIFY(tmpFile.open());
    const QString tmpFilePath = tmpFile.fileName();

    KIO::Job *getJob = KIO::file_copy(url, QUrl::fromLocalFile(tmpFilePath), -1, KIO::Overwrite | KIO::HideProgressInfo);
    bool ok = getJob->exec();
    QVERIFY2(ok, qPrintable(getJob->errorString()));
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
    const QString fileId = QStringLiteral("fileFromHome (1)");
    const QUrl url = TrashImpl::makeURL(0, fileId, QString());
    const QString infoFile(m_trashDir + QStringLiteral("/info/") + fileId + QStringLiteral(".trashinfo"));
    const QString filesItem(m_trashDir + QStringLiteral("/files/") + fileId);

    QVERIFY(QFile::exists(infoFile));
    QVERIFY(QFile::exists(filesItem));

    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << (int)3 << url;
    KIO::Job *job = KIO::special(url, packedArgs, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(ok);

    QVERIFY(!QFile::exists(infoFile));
    QVERIFY(!QFile::exists(filesItem));

    const QString destPath = homeTmpDir() + QStringLiteral("fileFromHome");
    QVERIFY(QFile::exists(destPath));
}

void TestTrash::restoreFileFromSubDir()
{
    const QString fileId = QStringLiteral("trashDirFromHome (1)/testfile");
    QVERIFY(!QFile::exists(homeTmpDir() + QStringLiteral("trashDirFromHome (1)")));

    const QUrl url = TrashImpl::makeURL(0, fileId, QString());
    const QString infoFile(m_trashDir + QStringLiteral("/info/trashDirFromHome (1).trashinfo"));
    const QString filesItem(m_trashDir + QStringLiteral("/files/trashDirFromHome (1)/testfile"));

    QVERIFY(QFile::exists(infoFile));
    QVERIFY(QFile::exists(filesItem));

    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << (int)3 << url;
    KIO::Job *job = KIO::special(url, packedArgs, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(!ok);
    // dest dir doesn't exist -> error message
    QCOMPARE(job->error(), KIO::ERR_SLAVE_DEFINED);

    // check that nothing happened
    QVERIFY(QFile::exists(infoFile));
    QVERIFY(QFile::exists(filesItem));
    QVERIFY(!QFile::exists(homeTmpDir() + QStringLiteral("trashDirFromHome (1)")));
}

void TestTrash::restoreFileToDeletedDirectory()
{
    // Ensure we'll get "fileFromHome" as fileId
    removeFile(m_trashDir, QStringLiteral("/info/fileFromHome.trashinfo"));
    removeFile(m_trashDir, QStringLiteral("/files/fileFromHome"));
    trashFileFromHome();
    // Delete orig dir
    KIO::Job *delJob = KIO::del(QUrl::fromLocalFile(homeTmpDir()), KIO::HideProgressInfo);
    bool delOK = delJob->exec();
    QVERIFY(delOK);

    const QString fileId = QStringLiteral("fileFromHome");
    const QUrl url = TrashImpl::makeURL(0, fileId, QString());
    const QString infoFile(m_trashDir + QStringLiteral("/info/") + fileId + QStringLiteral(".trashinfo"));
    const QString filesItem(m_trashDir + QStringLiteral("/files/") + fileId);

    QVERIFY(QFile::exists(infoFile));
    QVERIFY(QFile::exists(filesItem));

    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << (int)3 << url;
    KIO::Job *job = KIO::special(url, packedArgs, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(!ok);
    // dest dir doesn't exist -> error message
    QCOMPARE(job->error(), KIO::ERR_SLAVE_DEFINED);

    // check that nothing happened
    QVERIFY(QFile::exists(infoFile));
    QVERIFY(QFile::exists(filesItem));

    const QString destPath = homeTmpDir() + QStringLiteral("fileFromHome");
    QVERIFY(!QFile::exists(destPath));
}

void TestTrash::listRootDir()
{
    m_entryCount = 0;
    m_listResult.clear();
    m_displayNameListResult.clear();
    KIO::ListJob *job = KIO::listDir(QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    connect(job, &KIO::ListJob::entries,
            this, &TestTrash::slotEntries);
    bool ok = job->exec();
    QVERIFY(ok);
    qDebug() << "listDir done - m_entryCount=" << m_entryCount;
    QVERIFY(m_entryCount > 1);

    //qDebug() << m_listResult;
    //qDebug() << m_displayNameListResult;
    QCOMPARE(m_listResult.count(QStringLiteral(".")), 1);   // found it, and only once
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("fileFromHome")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("fileFromHome (1)")), 1);
}

void TestTrash::listRecursiveRootDir()
{
    m_entryCount = 0;
    m_listResult.clear();
    m_displayNameListResult.clear();
    KIO::ListJob *job = KIO::listRecursive(QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    connect(job, &KIO::ListJob::entries,
            this, &TestTrash::slotEntries);
    bool ok = job->exec();
    QVERIFY(ok);
    qDebug() << "listDir done - m_entryCount=" << m_entryCount;
    QVERIFY(m_entryCount > 1);

    qDebug() << m_listResult;
    qDebug() << m_displayNameListResult;
    QCOMPARE(m_listResult.count(QStringLiteral(".")), 1);   // found it, and only once
    QCOMPARE(m_listResult.count(QStringLiteral("0-fileFromHome")), 1);
    QCOMPARE(m_listResult.count(QStringLiteral("0-fileFromHome (1)")), 1);
    QCOMPARE(m_listResult.count(QStringLiteral("0-trashDirFromHome/testfile")), 1);
    QCOMPARE(m_listResult.count(QStringLiteral("0-readonly/readonly_subdir/testfile_in_subdir")), 1);
    QCOMPARE(m_listResult.count(QStringLiteral("0-subDirBrokenSymlink/link")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("fileFromHome")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("fileFromHome (1)")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("trashDirFromHome/testfile")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("readonly/readonly_subdir/testfile_in_subdir")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("subDirBrokenSymlink/link")), 1);
}

void TestTrash::listSubDir()
{
    m_entryCount = 0;
    m_listResult.clear();
    m_displayNameListResult.clear();
    KIO::ListJob *job = KIO::listDir(QUrl(QStringLiteral("trash:/0-trashDirFromHome")), KIO::HideProgressInfo);
    connect(job, &KIO::ListJob::entries,
            this, &TestTrash::slotEntries);
    bool ok = job->exec();
    QVERIFY(ok);
    qDebug() << "listDir done - m_entryCount=" << m_entryCount;
    QCOMPARE(m_entryCount, 3);

    //qDebug() << m_listResult;
    //qDebug() << m_displayNameListResult;
    QCOMPARE(m_listResult.count(QStringLiteral(".")), 1);   // found it, and only once
    QCOMPARE(m_listResult.count(QStringLiteral("testfile")), 1);   // found it, and only once
    QCOMPARE(m_listResult.count(QStringLiteral("subdir")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("testfile")), 1);
    QCOMPARE(m_displayNameListResult.count(QStringLiteral("subdir")), 1);
}

void TestTrash::slotEntries(KIO::Job *, const KIO::UDSEntryList &lst)
{
    for (const KIO::UDSEntry &entry : lst) {
        QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
        QString displayName = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
        QUrl url(entry.stringValue(KIO::UDSEntry::UDS_URL));
        qDebug() << "name" << name << "displayName" << displayName << " UDS_URL=" << url;
        if (!url.isEmpty()) {
            QCOMPARE(url.scheme(), QStringLiteral("trash"));
        }
        m_listResult << name;
        m_displayNameListResult << displayName;
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
    createTestFile(m_trashDir + "/files/testfile_nometadata");

    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << (int)1;
    KIO::Job *job = KIO::special(QUrl("trash:/"), packedArgs, KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(ok);

    KConfig cfg("trashrc", KConfig::SimpleConfig);
    QVERIFY(cfg.hasGroup("Status"));
    QVERIFY(cfg.group("Status").readEntry("Empty", false) == true);

    QVERIFY(!QFile::exists(m_trashDir + "/files/fileFromHome"));
    QVERIFY(!QFile::exists(m_trashDir + "/files/readonly"));
    QVERIFY(!QFile::exists(m_trashDir + "/info/readonly.trashinfo"));
    QVERIFY(QDir(m_trashDir + "/info").entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty());
    QVERIFY(QDir(m_trashDir + "/files").entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty());

#else
    qDebug() << " : SKIPPED";
#endif
}

static bool isTrashEmpty()
{
    KConfig cfg(QStringLiteral("trashrc"), KConfig::SimpleConfig);
    const KConfigGroup group = cfg.group("Status");
    return group.readEntry("Empty", true);
}

void TestTrash::testEmptyTrashSize()
{
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl(QStringLiteral("trash:/")));
    QVERIFY(job->exec());
    if (isTrashEmpty()) {
        QCOMPARE(job->totalSize(), 0ULL);
    } else {
        QVERIFY(job->totalSize() < 1000000000 /*1GB*/); // #157023
    }
}

static void checkIcon(const QUrl &url, const QString &expectedIcon)
{
    QString icon = KIO::iconNameForUrl(url); // #100321
    QCOMPARE(icon, expectedIcon);
}

void TestTrash::testIcons()
{
    // The JSON file says "user-trash-full" in all cases, whether the trash is full or not
    QCOMPARE(KProtocolInfo::icon(QStringLiteral("trash")), QStringLiteral("user-trash-full"));  // #100321

    if (isTrashEmpty()) {
        checkIcon(QUrl(QStringLiteral("trash:/")), QStringLiteral("user-trash"));
    } else {
        checkIcon(QUrl(QStringLiteral("trash:/")), QStringLiteral("user-trash-full"));
    }

    checkIcon(QUrl(QStringLiteral("trash:/foo/")), QStringLiteral("inode-directory"));
}

QTEST_GUILESS_MAIN(TestTrash)

