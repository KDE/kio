/* This file is part of the KDE project
   Copyright (C) 2004-2006 David Faure <faure@kde.org>

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

#include "jobtest.h"

#include <klocalizedstring.h>

#include <QDebug>
#include <QPointer>
#include <QSignalSpy>
#include <QFileInfo>
#include <QEventLoop>
#include <QDir>
#include <QHash>
#include <QVariant>
#include <QBuffer>
#include <QTemporaryFile>
#include <QTest>
#include <QUrl>

#include <kprotocolinfo.h>
#include <kio/scheduler.h>
#include <kio/directorysizejob.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/chmodjob.h>
#include "kiotesthelper.h" // createTestFile etc.
#ifndef Q_OS_WIN
#include <unistd.h> // for readlink
#endif

QTEST_MAIN(JobTest)

// The code comes partly from kdebase/kioslave/trash/testtrash.cpp

static QString otherTmpDir()
{
#ifdef Q_OS_WIN
    return QDir::tempPath() + "/jobtest/";
#else
    // This one needs to be on another partition
    return QStringLiteral("/tmp/jobtest/");
#endif
}

#if 0
static QUrl systemTmpDir()
{
#ifdef Q_OS_WIN
    return QUrl("system:" + QDir::homePath() + "/.kde-unit-test/jobtest-system/");
#else
    return QUrl("system:/home/.kde-unit-test/jobtest-system/");
#endif
}

static QString realSystemPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/jobtest-system/";
}
#endif

void JobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::instance()->setApplicationName("kio/jobtest"); // testing for #357499

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");

    s_referenceTimeStamp = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago

    // Start with a clean base dir
    cleanupTestCase();
    homeTmpDir(); // create it
    if (!QFile::exists(otherTmpDir())) {
        bool ok = QDir().mkdir(otherTmpDir());
        if (!ok) {
            qFatal("couldn't create %s", qPrintable(otherTmpDir()));
        }
    }
#if 0
    if (KProtocolInfo::isKnownProtocol("system")) {
        if (!QFile::exists(realSystemPath())) {
            bool ok = dir.mkdir(realSystemPath());
            if (!ok) {
                qFatal("couldn't create %s", qPrintable(realSystemPath()));
            }
        }
    }
#endif

    qRegisterMetaType<KJob *>("KJob*");
    qRegisterMetaType<KIO::Job *>("KIO::Job*");
    qRegisterMetaType<QDateTime>("QDateTime");
}

void JobTest::cleanupTestCase()
{
    QDir(homeTmpDir()).removeRecursively();
    QDir(otherTmpDir()).removeRecursively();
#if 0
    if (KProtocolInfo::isKnownProtocol("system")) {
        delDir(systemTmpDir());
    }
#endif
}

void JobTest::enterLoop()
{
    QEventLoop eventLoop;
    connect(this, SIGNAL(exitLoop()),
            &eventLoop, SLOT(quit()));
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

void JobTest::storedGet()
{
    qDebug();
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    QUrl u = QUrl::fromLocalFile(filePath);
    m_result = -1;

    KIO::StoredTransferJob *job = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, SIGNAL(percent(KJob*,ulong)));
    QVERIFY(spyPercent.isValid());
    job->setUiDelegate(nullptr);
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotGetResult(KJob*)));
    enterLoop();
    QCOMPARE(m_result, 0);   // no error
    QCOMPARE(m_data, QByteArray("Hello\0world", 11));
    QCOMPARE(m_data.size(), 11);
    QVERIFY(!spyPercent.isEmpty());
}

void JobTest::slotGetResult(KJob *job)
{
    m_result = job->error();
    m_data = static_cast<KIO::StoredTransferJob *>(job)->data();
    emit exitLoop();
}

void JobTest::put()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    QUrl u = QUrl::fromLocalFile(filePath);
    KIO::TransferJob *job = KIO::put(u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QDateTime mtime = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    job->setModificationTime(mtime);
    job->setUiDelegate(nullptr);
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)));
    connect(job, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
            this, SLOT(slotDataReq(KIO::Job*,QByteArray&)));
    m_result = -1;
    m_dataReqCount = 0;
    enterLoop();
    QVERIFY(m_result == 0);   // no error

    QFileInfo fileInfo(filePath);
    QVERIFY(fileInfo.exists());
    QCOMPARE(fileInfo.size(), 30LL); // "This is a test for KIO::put()\n"
    QCOMPARE((int)fileInfo.permissions(), (int)(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    QCOMPARE(fileInfo.lastModified(), mtime);
}

void JobTest::slotDataReq(KIO::Job *, QByteArray &data)
{
    // Really not the way you'd write a slotDataReq usually :)
    switch (m_dataReqCount++) {
    case 0:
        data = "This is a test for ";
        break;
    case 1:
        data = "KIO::put()\n";
        break;
    case 2:
        data = QByteArray();
        break;
    }
}

void JobTest::slotResult(KJob *job)
{
    m_result = job->error();
    emit exitLoop();
}

void JobTest::storedPut()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    QUrl u = QUrl::fromLocalFile(filePath);
    QByteArray putData = "This is the put data";
    KIO::TransferJob *job = KIO::storedPut(putData, u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, SIGNAL(percent(KJob*,ulong)));
    QVERIFY(spyPercent.isValid());
    QDateTime mtime = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    job->setModificationTime(mtime);
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());
    QFileInfo fileInfo(filePath);
    QVERIFY(fileInfo.exists());
    QCOMPARE(fileInfo.size(), (long long)putData.size());
    QCOMPARE((int)fileInfo.permissions(), (int)(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    QCOMPARE(fileInfo.lastModified(), mtime);
    QVERIFY(!spyPercent.isEmpty());
}

void JobTest::storedPutIODevice()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    QBuffer putData;
    putData.setData("This is the put data");
    QVERIFY(putData.open(QIODevice::ReadOnly));
    KIO::TransferJob *job = KIO::storedPut(&putData, QUrl::fromLocalFile(filePath), 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, SIGNAL(percent(KJob*,ulong)));
    QVERIFY(spyPercent.isValid());
    QDateTime mtime = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    job->setModificationTime(mtime);
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());
    QFileInfo fileInfo(filePath);
    QVERIFY(fileInfo.exists());
    QCOMPARE(fileInfo.size(), (long long)putData.size());
    QCOMPARE((int)fileInfo.permissions(), (int)(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    QCOMPARE(fileInfo.lastModified(), mtime);
    QVERIFY(!spyPercent.isEmpty());
}

void JobTest::storedPutIODeviceFile()
{
    // Given a source file and a destination file
    const QString src = homeTmpDir() + "fileFromHome";
    createTestFile(src);
    QVERIFY(QFile::exists(src));
    QFile srcFile(src);
    QVERIFY(srcFile.open(QIODevice::ReadOnly));
    const QString dest = homeTmpDir() + "fileFromHome_copied";
    QFile::remove(dest);
    const QUrl destUrl = QUrl::fromLocalFile(dest);

    // When using storedPut with the QFile as argument
    KIO::StoredTransferJob *job = KIO::storedPut(&srcFile, destUrl, 0600, KIO::Overwrite | KIO::HideProgressInfo);

    // Then the copy should succeed and the dest file exist
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest));
    QCOMPARE(QFileInfo(src).size(), QFileInfo(dest).size());
    QFile::remove(dest);
}

void JobTest::storedPutIODeviceTempFile()
{
    // Create a temp file in the current dir.
    QTemporaryFile tempFile(QStringLiteral("jobtest-tmp"));
    QVERIFY(tempFile.open());

    // Write something into the file.
    QTextStream stream(&tempFile);
    stream << QStringLiteral("This is the put data");
    stream.flush();
    QVERIFY(QFileInfo(tempFile).size() > 0);

    const QString dest = homeTmpDir() + QLatin1String("tmpfile-dest");
    const QUrl destUrl = QUrl::fromLocalFile(dest);

    // QTemporaryFiles are open in ReadWrite mode,
    // so we don't need to close and reopen,
    // but we need to rewind to the beginning.
    tempFile.seek(0);
    auto job = KIO::storedPut(&tempFile, destUrl, -1);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFileInfo::exists(dest));
    QCOMPARE(QFileInfo(dest).size(), QFileInfo(tempFile).size());
    QVERIFY(QFile::remove(dest));
}

void JobTest::storedPutIODeviceFastDevice()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QUrl u = QUrl::fromLocalFile(filePath);
    const QByteArray putDataContents = "This is the put data";
    QBuffer putDataBuffer;
    QVERIFY(putDataBuffer.open(QIODevice::ReadWrite));

    KIO::StoredTransferJob *job = KIO::storedPut(&putDataBuffer, u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, SIGNAL(percent(KJob*,ulong)));
    QVERIFY(spyPercent.isValid());
    QDateTime mtime = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    job->setModificationTime(mtime);
    job->setTotalSize(putDataContents.size());
    job->setUiDelegate(nullptr);
    job->setAsyncDataEnabled(true);

    // Emit the readChannelFinished even before the job has had time to start
    const auto pos = putDataBuffer.pos();
    int size = putDataBuffer.write(putDataContents);
    putDataBuffer.seek(pos);
    putDataBuffer.readChannelFinished();

    QVERIFY(job->exec());
    QCOMPARE(size, putDataContents.size());
    QCOMPARE(putDataBuffer.bytesAvailable(), 0);

    QFileInfo fileInfo(filePath);
    QVERIFY(fileInfo.exists());
    QCOMPARE(fileInfo.size(), (long long)putDataContents.size());
    QCOMPARE((int)fileInfo.permissions(), (int)(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    QCOMPARE(fileInfo.lastModified(), mtime);
    QVERIFY(!spyPercent.isEmpty());
}

void JobTest::storedPutIODeviceSlowDevice()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QUrl u = QUrl::fromLocalFile(filePath);
    const QByteArray putDataContents = "This is the put data";
    QBuffer putDataBuffer;
    QVERIFY(putDataBuffer.open(QIODevice::ReadWrite));

    KIO::StoredTransferJob *job = KIO::storedPut(&putDataBuffer, u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, SIGNAL(percent(KJob*,ulong)));
    QVERIFY(spyPercent.isValid());
    QDateTime mtime = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    job->setModificationTime(mtime);
    job->setTotalSize(putDataContents.size());
    job->setUiDelegate(nullptr);
    job->setAsyncDataEnabled(true);

    int size = 0;
    const auto writeOnce = [&putDataBuffer, &size, putDataContents]() {
        const auto pos = putDataBuffer.pos();
        size += putDataBuffer.write(putDataContents);
        putDataBuffer.seek(pos);
//         qDebug() << "written" << size;
    };

    QTimer::singleShot(200, this, writeOnce);
    QTimer::singleShot(400, this, writeOnce);
    // Simulate the transfer is done
    QTimer::singleShot(450, this, [&putDataBuffer](){ putDataBuffer.readChannelFinished(); });

    QVERIFY(job->exec());
    QCOMPARE(size, putDataContents.size() * 2);
    QCOMPARE(putDataBuffer.bytesAvailable(), 0);

    QFileInfo fileInfo(filePath);
    QVERIFY(fileInfo.exists());
    QCOMPARE(fileInfo.size(), (long long)putDataContents.size() * 2);
    QCOMPARE((int)fileInfo.permissions(), (int)(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    QCOMPARE(fileInfo.lastModified(), mtime);
    QVERIFY(!spyPercent.isEmpty());
}

void JobTest::storedPutIODeviceSlowDeviceBigChunk()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QUrl u = QUrl::fromLocalFile(filePath);
    const QByteArray putDataContents(300000, 'K'); // Make sure the 300000 is bigger than MAX_READ_BUF_SIZE
    QBuffer putDataBuffer;
    QVERIFY(putDataBuffer.open(QIODevice::ReadWrite));

    KIO::StoredTransferJob *job = KIO::storedPut(&putDataBuffer, u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, SIGNAL(percent(KJob*,ulong)));
    QVERIFY(spyPercent.isValid());
    QDateTime mtime = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    job->setModificationTime(mtime);
    job->setTotalSize(putDataContents.size());
    job->setUiDelegate(nullptr);
    job->setAsyncDataEnabled(true);

    int size = 0;
    const auto writeOnce = [&putDataBuffer, &size, putDataContents]() {
        const auto pos = putDataBuffer.pos();
        size += putDataBuffer.write(putDataContents);
        putDataBuffer.seek(pos);
//         qDebug() << "written" << size;
    };

    QTimer::singleShot(200, this, writeOnce);
    // Simulate the transfer is done
    QTimer::singleShot(450, this, [&putDataBuffer](){ putDataBuffer.readChannelFinished(); });

    QVERIFY(job->exec());
    QCOMPARE(size, putDataContents.size());
    QCOMPARE(putDataBuffer.bytesAvailable(), 0);

    QFileInfo fileInfo(filePath);
    QVERIFY(fileInfo.exists());
    QCOMPARE(fileInfo.size(), (long long)putDataContents.size());
    QCOMPARE((int)fileInfo.permissions(), (int)(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser));
    QCOMPARE(fileInfo.lastModified(), mtime);
    QVERIFY(!spyPercent.isEmpty());
}

void JobTest::asyncStoredPutReadyReadAfterFinish()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QUrl u = QUrl::fromLocalFile(filePath);

    QBuffer putDataBuffer;
    QVERIFY(putDataBuffer.open(QIODevice::ReadWrite));

    KIO::StoredTransferJob *job = KIO::storedPut(&putDataBuffer, u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    job->setAsyncDataEnabled(true);

    bool jobFinished = false;

    connect(job, &KJob::finished, [&jobFinished, &putDataBuffer] {
        putDataBuffer.readyRead();
        jobFinished = true;
    });

    QTimer::singleShot(200, this, [job]() {
        job->kill();
    });

    QTRY_VERIFY(jobFinished);
}

////

void JobTest::copyLocalFile(const QString &src, const QString &dest)
{
    const QUrl u = QUrl::fromLocalFile(src);
    const QUrl d = QUrl::fromLocalFile(dest);

    const int perms = 0666;
    // copy the file with file_copy
    KIO::Job *job = KIO::file_copy(u, d, perms, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there
    QCOMPARE(int(QFileInfo(dest).permissions()), int(0x6666));

    {
        // check that the timestamp is the same (#24443)
        // Note: this only works because of copy() in kio_file.
        // The datapump solution ignores mtime, the app has to call FileCopyJob::setModificationTime()
        QFileInfo srcInfo(src);
        QFileInfo destInfo(dest);
#ifdef Q_OS_WIN
        // win32 time may differs in msec part
        QCOMPARE(srcInfo.lastModified().toString("dd.MM.yyyy hh:mm"),
                 destInfo.lastModified().toString("dd.MM.yyyy hh:mm"));
#else
        QCOMPARE(srcInfo.lastModified(), destInfo.lastModified());
#endif
    }

    // cleanup and retry with KIO::copy()
    QFile::remove(dest);
    job = KIO::copy(u, d, KIO::HideProgressInfo);
    QSignalSpy spyCopyingDone(job, SIGNAL(copyingDone(KIO::Job*,QUrl,QUrl,QDateTime,bool,bool)));
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    ok = job->exec();
    QVERIFY(ok);
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there
    {
        // check that the timestamp is the same (#24443)
        QFileInfo srcInfo(src);
        QFileInfo destInfo(dest);
#ifdef Q_OS_WIN
        // win32 time may differs in msec part
        QCOMPARE(srcInfo.lastModified().toString("dd.MM.yyyy hh:mm"),
                 destInfo.lastModified().toString("dd.MM.yyyy hh:mm"));
#else
        QCOMPARE(srcInfo.lastModified(), destInfo.lastModified());
#endif
    }
    QCOMPARE(spyCopyingDone.count(), 1);

    // cleanup and retry with KIO::copyAs()
    QFile::remove(dest);
    job = KIO::copyAs(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->exec());
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there

    // Do it again, with Overwrite.
    job = KIO::copyAs(u, d, KIO::Overwrite | KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->exec());
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there

    // Do it again, without Overwrite (should fail).
    job = KIO::copyAs(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(!job->exec());

    // Clean up
    QFile::remove(dest);
}

void JobTest::copyLocalDirectory(const QString &src, const QString &_dest, int flags)
{
    QVERIFY(QFileInfo(src).isDir());
    QVERIFY(QFileInfo(src + "/testfile").isFile());
    QUrl u = QUrl::fromLocalFile(src);
    QString dest(_dest);
    QUrl d = QUrl::fromLocalFile(dest);
    if (flags & AlreadyExists) {
        QVERIFY(QFile::exists(dest));
    } else {
        QVERIFY(!QFile::exists(dest));
    }

    KIO::Job *job = KIO::copy(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFileInfo(dest).isDir());
    QVERIFY(QFileInfo(dest + "/testfile").isFile());
    QVERIFY(QFile::exists(src));     // still there

    if (flags & AlreadyExists) {
        dest += '/' + u.fileName();
        //qDebug() << "Expecting dest=" << dest;
    }

    // CopyJob::setNextDirAttribute isn't implemented for Windows currently.
#ifndef Q_OS_WIN
    {
        // Check that the timestamp is the same (#24443)
        QFileInfo srcInfo(src);
        QFileInfo destInfo(dest);
        QCOMPARE(srcInfo.lastModified(), destInfo.lastModified());
    }
#endif

    // Do it again, with Overwrite.
    // Use copyAs, we don't want a subdir inside d.
    job = KIO::copyAs(u, d, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    ok = job->exec();
    QVERIFY(ok);

    // Do it again, without Overwrite (should fail).
    job = KIO::copyAs(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    ok = job->exec();
    QVERIFY(!ok);
}

#ifndef Q_OS_WIN
static QString linkTarget(const QString &path)
{
    // Use readlink on Unix because symLinkTarget turns relative targets into absolute (#352927)
    char linkTargetBuffer[4096];
    const int n = readlink(QFile::encodeName(path).constData(), linkTargetBuffer, sizeof(linkTargetBuffer) - 1);
    if (n != -1) {
        linkTargetBuffer[n] = 0;
    }
    return QFile::decodeName(linkTargetBuffer);
}

static void copyLocalSymlink(const QString &src, const QString &dest, const QString &expectedLinkTarget)
{
    QT_STATBUF buf;
    QVERIFY(QT_LSTAT(QFile::encodeName(src).constData(), &buf) == 0);
    QUrl u = QUrl::fromLocalFile(src);
    QUrl d = QUrl::fromLocalFile(dest);

    // copy the symlink
    KIO::Job *job = KIO::copy(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY2(job->exec(), qPrintable(QString::number(job->error())));
    QVERIFY(QT_LSTAT(QFile::encodeName(dest).constData(), &buf) == 0); // dest exists
    QCOMPARE(linkTarget(dest), expectedLinkTarget);

    // cleanup
    QFile::remove(dest);
}
#endif

void JobTest::copyFileToSamePartition()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QString dest = homeTmpDir() + "fileFromHome_copied";
    createTestFile(filePath);
    copyLocalFile(filePath, dest);
}

void JobTest::copyDirectoryToSamePartition()
{
    qDebug();
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = homeTmpDir() + "dirFromHome_copied";
    createTestDirectory(src);
    copyLocalDirectory(src, dest);
}

void JobTest::copyDirectoryToExistingDirectory()
{
    qDebug();
    // just the same as copyDirectoryToSamePartition, but this time dest exists.
    // So we get a subdir, "dirFromHome_copy/dirFromHome"
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = homeTmpDir() + "dirFromHome_copied";
    createTestDirectory(src);
    createTestDirectory(dest);
    copyLocalDirectory(src, dest, AlreadyExists);
}

void JobTest::copyFileToOtherPartition()
{
    qDebug();
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QString dest = otherTmpDir() + "fileFromHome_copied";
    createTestFile(filePath);
    copyLocalFile(filePath, dest);
}

void JobTest::copyDirectoryToOtherPartition()
{
    qDebug();
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = otherTmpDir() + "dirFromHome_copied";
    createTestDirectory(src);
    copyLocalDirectory(src, dest);
}

void JobTest::copyRelativeSymlinkToSamePartition() // #352927
{
#ifdef Q_OS_WIN
    QSKIP("Skipping symlink test on Windows");
#else
    const QString filePath = homeTmpDir() + "testlink";
    const QString dest = homeTmpDir() + "testlink_copied";
    createTestSymlink(filePath, "relative");
    copyLocalSymlink(filePath, dest, QStringLiteral("relative"));
    QFile::remove(filePath);
#endif
}

void JobTest::copyAbsoluteSymlinkToOtherPartition()
{
#ifdef Q_OS_WIN
    QSKIP("Skipping symlink test on Windows");
#else
    const QString filePath = homeTmpDir() + "testlink";
    const QString dest = otherTmpDir() + "testlink_copied";
    createTestSymlink(filePath, QFile::encodeName(homeTmpDir()));
    copyLocalSymlink(filePath, dest, homeTmpDir());
    QFile::remove(filePath);
#endif
}

void JobTest::copyFolderWithUnaccessibleSubfolder()
{
#ifdef Q_OS_WIN
    QSKIP("Skipping unaccessible folder test on Windows, cannot remove all permissions from a folder");
#endif
    const QString src_dir = homeTmpDir() + "srcHome";
    const QString dst_dir = homeTmpDir() + "dstHome";

    QDir().remove(src_dir);
    QDir().remove(dst_dir);

    createTestDirectory(src_dir);
    createTestDirectory(src_dir + "/folder1");
    QString inaccessible = src_dir + "/folder1/inaccessible";
    createTestDirectory(inaccessible);

    QFile(inaccessible).setPermissions(QFile::Permissions()); // Make it inaccessible
    //Copying should throw some warnings, as it cannot access some folders

    KIO::CopyJob *job = KIO::copy(QUrl::fromLocalFile(src_dir), QUrl::fromLocalFile(dst_dir), KIO::HideProgressInfo);

    QSignalSpy spy(job, SIGNAL(warning(KJob*,QString,QString)));
    job->setUiDelegate(nullptr);   // no skip dialog, thanks
    QVERIFY(job->exec());

    QFile(inaccessible).setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

    KIO::DeleteJob *deljob1 = KIO::del(QUrl::fromLocalFile(src_dir), KIO::HideProgressInfo);
    deljob1->setUiDelegate(nullptr); // no skip dialog, thanks
    QVERIFY(deljob1->exec());

    KIO::DeleteJob *deljob2 = KIO::del(QUrl::fromLocalFile(dst_dir), KIO::HideProgressInfo);
    deljob2->setUiDelegate(nullptr); // no skip dialog, thanks
    QVERIFY(deljob2->exec());

    QCOMPARE(spy.count(), 1); // one warning should be emitted by the copy job
}

void JobTest::copyDataUrl()
{
    // GIVEN
    const QString dst_dir = homeTmpDir();
    QVERIFY(!QFileInfo::exists(dst_dir + "/data"));
    // WHEN
    KIO::CopyJob *job = KIO::copy(QUrl("data:,Hello%2C%20World!"), QUrl::fromLocalFile(dst_dir), KIO::HideProgressInfo);
    QVERIFY(job->exec());
    // THEN
    QVERIFY(QFileInfo(dst_dir + "/data").isFile());
    QFile::remove(dst_dir + "/data");
}

void JobTest::suspendFileCopy()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QString dest = homeTmpDir() + "fileFromHome_copied";
    createTestFile(filePath);

    const QUrl u = QUrl::fromLocalFile(filePath);
    const QUrl d = QUrl::fromLocalFile(dest);
    KIO::Job *job = KIO::file_copy(u, d, KIO::HideProgressInfo);
    QSignalSpy spyResult(job, SIGNAL(result(KJob*)));
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->suspend());
    QVERIFY(!spyResult.wait(300));
    QVERIFY(job->resume());
    QVERIFY(job->exec());
    QVERIFY(QFile::exists(dest));
    QFile::remove(dest);
}

void JobTest::suspendCopy()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QString dest = homeTmpDir() + "fileFromHome_copied";
    createTestFile(filePath);

    const QUrl u = QUrl::fromLocalFile(filePath);
    const QUrl d = QUrl::fromLocalFile(dest);
    KIO::Job *job = KIO::copy(u, d, KIO::HideProgressInfo);
    QSignalSpy spyResult(job, SIGNAL(result(KJob*)));
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->suspend());
    QVERIFY(!spyResult.wait(300));
    QVERIFY(job->resume());
    QVERIFY(job->exec());
    QVERIFY(QFile::exists(dest));
    QFile::remove(dest);
}

void JobTest::moveLocalFile(const QString &src, const QString &dest)
{
    QVERIFY(QFile::exists(src));
    QUrl u = QUrl::fromLocalFile(src);
    QUrl d = QUrl::fromLocalFile(dest);

    // move the file with file_move
    KIO::Job *job = KIO::file_move(u, d, 0666, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(QFile::exists(dest));
    QVERIFY(!QFile::exists(src));     // not there anymore
    QCOMPARE(int(QFileInfo(dest).permissions()), int(0x6666));

    // move it back with KIO::move()
    job = KIO::move(d, u, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // it's back
}

static void moveLocalSymlink(const QString &src, const QString &dest)
{
    QT_STATBUF buf;
    QVERIFY(QT_LSTAT(QFile::encodeName(src).constData(), &buf) == 0);
    QUrl u = QUrl::fromLocalFile(src);
    QUrl d = QUrl::fromLocalFile(dest);

    // move the symlink with move, NOT with file_move
    KIO::Job *job = KIO::move(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    bool ok = job->exec();
    if (!ok) {
        qWarning() << job->error();
    }
    QVERIFY(ok);
    QVERIFY(QT_LSTAT(QFile::encodeName(dest).constData(), &buf) == 0);
    QVERIFY(!QFile::exists(src));     // not there anymore

    // move it back with KIO::move()
    job = KIO::move(d, u, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    ok = job->exec();
    QVERIFY(ok);
    QVERIFY(QT_LSTAT(QFile::encodeName(dest).constData(), &buf) != 0); // doesn't exist anymore
    QVERIFY(QT_LSTAT(QFile::encodeName(src).constData(), &buf) == 0); // it's back
}

void JobTest::moveLocalDirectory(const QString &src, const QString &dest)
{
    qDebug() << src << " " << dest;
    QVERIFY(QFile::exists(src));
    QVERIFY(QFileInfo(src).isDir());
    QVERIFY(QFileInfo(src + "/testfile").isFile());
#ifndef Q_OS_WIN
    QVERIFY(QFileInfo(src + "/testlink").isSymLink());
#endif
    QUrl u = QUrl::fromLocalFile(src);
    QUrl d = QUrl::fromLocalFile(dest);

    KIO::Job *job = KIO::move(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    bool ok = job->exec();
    QVERIFY2(ok, qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFileInfo(dest).isDir());
    QVERIFY(QFileInfo(dest + "/testfile").isFile());
    QVERIFY(!QFile::exists(src));     // not there anymore
#ifndef Q_OS_WIN
    QVERIFY(QFileInfo(dest + "/testlink").isSymLink());
#endif
}

void JobTest::moveFileToSamePartition()
{
    qDebug();
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QString dest = homeTmpDir() + "fileFromHome_moved";
    createTestFile(filePath);
    moveLocalFile(filePath, dest);
}

void JobTest::moveDirectoryToSamePartition()
{
    qDebug();
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = homeTmpDir() + "dirFromHome_moved";
    createTestDirectory(src);
    moveLocalDirectory(src, dest);
}

void JobTest::moveDirectoryIntoItself()
{
    qDebug();
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = src + "/foo";
    createTestDirectory(src);
    QVERIFY(QFile::exists(src));
    QUrl u = QUrl::fromLocalFile(src);
    QUrl d = QUrl::fromLocalFile(dest);
    KIO::CopyJob *job = KIO::move(u, d);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_CANNOT_MOVE_INTO_ITSELF);
    QCOMPARE(job->errorString(), i18n("A folder cannot be moved into itself"));
    QDir(dest).removeRecursively();
}

void JobTest::moveFileToOtherPartition()
{
    qDebug();
    const QString filePath = homeTmpDir() + "fileFromHome";
    const QString dest = otherTmpDir() + "fileFromHome_moved";
    createTestFile(filePath);
    moveLocalFile(filePath, dest);
}

void JobTest::moveSymlinkToOtherPartition()
{
#ifndef Q_OS_WIN
    qDebug();
    const QString filePath = homeTmpDir() + "testlink";
    const QString dest = otherTmpDir() + "testlink_moved";
    createTestSymlink(filePath);
    moveLocalSymlink(filePath, dest);
#endif
}

void JobTest::moveDirectoryToOtherPartition()
{
    qDebug();
#ifndef Q_OS_WIN
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = otherTmpDir() + "dirFromHome_moved";
    createTestDirectory(src);
    moveLocalDirectory(src, dest);
#endif
}

void JobTest::moveFileNoPermissions()
{
#ifdef Q_OS_WIN
    QSKIP("Skipping unaccessible folder test on Windows, cannot remove all permissions from a folder");
#endif
    // Given a file that cannot be moved (subdir has no permissions)
    const QString subdir = homeTmpDir() + "subdir";
    QVERIFY(QDir().mkpath(subdir));
    const QString src = subdir + "/thefile";
    createTestFile(src);
    QVERIFY(QFile(subdir).setPermissions(QFile::Permissions())); // Make it inaccessible

    // When trying to move it
    const QString dest = homeTmpDir() + "dest";
    KIO::CopyJob *job = KIO::move(QUrl::fromLocalFile(src), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr); // no skip dialog, thanks

    // The job should fail with "access denied"
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_ACCESS_DENIED);
    // Note that, just like mv(1), KIO's behavior depends on whether
    // a direct rename(2) was used, or a full copy+del. In the first case
    // there is no destination file created, but in the second case the
    // destination file remains.
    // In this test it's the same partition, so no dest created.
    QVERIFY(!QFile::exists(dest));

    // Cleanup
    QVERIFY(QFile(subdir).setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)));
    QVERIFY(QFile::exists(src));
    QVERIFY(QDir(subdir).removeRecursively());
}

void JobTest::moveDirectoryNoPermissions()
{
#ifdef Q_OS_WIN
    QSKIP("Skipping unaccessible folder test on Windows, cannot remove all permissions from a folder");
#endif
    // Given a dir that cannot be moved (parent dir has no permissions)
    const QString subdir = homeTmpDir() + "subdir";
    const QString src = subdir + "/thedir";
    QVERIFY(QDir().mkpath(src));
    QVERIFY(QFileInfo(src).isDir());
    QVERIFY(QFile(subdir).setPermissions(QFile::Permissions())); // Make it inaccessible

    // When trying to move it
    const QString dest = homeTmpDir() + "mdnp";
    KIO::CopyJob *job = KIO::move(QUrl::fromLocalFile(src), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr); // no skip dialog, thanks

    // The job should fail with "access denied"
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_ACCESS_DENIED);

    QVERIFY(!QFile::exists(dest));

    // Cleanup
    QVERIFY(QFile(subdir).setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)));
    QVERIFY(QFile::exists(src));
    QVERIFY(QDir(subdir).removeRecursively());
}

void JobTest::listRecursive()
{
    // Note: many other tests must have been run before since we rely on the files they created

    const QString src = homeTmpDir();
#ifndef Q_OS_WIN
    // Add a symlink to a dir, to make sure we don't recurse into those
    bool symlinkOk = symlink("dirFromHome", QFile::encodeName(src + "/dirFromHome_link").constData()) == 0;
    QVERIFY(symlinkOk);
#endif
    KIO::ListJob *job = KIO::listRecursive(QUrl::fromLocalFile(src), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    connect(job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
            SLOT(slotEntries(KIO::Job*,KIO::UDSEntryList)));
    bool ok = job->exec();
    QVERIFY(ok);
    m_names.sort();
    QByteArray ref_names = QByteArray(".,..,"
                                      "dirFromHome,dirFromHome/testfile,"
                                      "dirFromHome/testlink," // exists on Windows too, see createTestDirectory
                                      "dirFromHome_copied,"
                                      "dirFromHome_copied/dirFromHome,dirFromHome_copied/dirFromHome/testfile,"
                                      "dirFromHome_copied/dirFromHome/testlink,"
                                      "dirFromHome_copied/testfile,"
                                      "dirFromHome_copied/testlink,"
#ifndef Q_OS_WIN
                                      "dirFromHome_link,"
#endif
                                      "fileFromHome");

    const QString joinedNames = m_names.join(QStringLiteral(","));
    if (joinedNames.toLatin1() != ref_names) {
        qDebug("%s", qPrintable(joinedNames));
        qDebug("%s", ref_names.data());
    }
    QCOMPARE(joinedNames.toLatin1(), ref_names);
}

void JobTest::listFile()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    KIO::ListJob *job = KIO::listDir(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), static_cast<int>(KIO::ERR_IS_FILE));

    // And list something that doesn't exist
    const QString path = homeTmpDir() + "fileFromHomeDoesNotExist";
    job = KIO::listDir(QUrl::fromLocalFile(path), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), static_cast<int>(KIO::ERR_DOES_NOT_EXIST));
}

void JobTest::killJob()
{
    const QString src = homeTmpDir();
    KIO::ListJob *job = KIO::listDir(QUrl::fromLocalFile(src), KIO::HideProgressInfo);
    QVERIFY(job->isAutoDelete());
    QPointer<KIO::ListJob> ptr(job);
    job->setUiDelegate(nullptr);
    qApp->processEvents(); // let the job start, it's no fun otherwise
    job->kill();
    qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete); // process the deferred delete of the job
    QVERIFY(ptr.isNull());
}

void JobTest::killJobBeforeStart()
{
    const QString src = homeTmpDir();
    KIO::Job *job = KIO::stat(QUrl::fromLocalFile(src), KIO::HideProgressInfo);
    QVERIFY(job->isAutoDelete());
    QPointer<KIO::Job> ptr(job);
    job->setUiDelegate(nullptr);
    job->kill();
    qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete); // process the deferred delete of the job
    QVERIFY(ptr.isNull());
    qApp->processEvents(); // does KIO scheduler crash here? nope.
}

void JobTest::deleteJobBeforeStart() // #163171
{
    const QString src = homeTmpDir();
    KIO::Job *job = KIO::stat(QUrl::fromLocalFile(src), KIO::HideProgressInfo);
    QVERIFY(job->isAutoDelete());
    job->setUiDelegate(nullptr);
    delete job;
    qApp->processEvents(); // does KIO scheduler crash here?
}

void JobTest::directorySize()
{
    // Note: many other tests must have been run before since we rely on the files they created

    const QString src = homeTmpDir();

    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(src));
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    qDebug() << "totalSize: " << job->totalSize();
    qDebug() << "totalFiles: " << job->totalFiles();
    qDebug() << "totalSubdirs: " << job->totalSubdirs();
#ifdef Q_OS_WIN
    QCOMPARE(job->totalFiles(), 5ULL); // see expected result in listRecursive() above
    QCOMPARE(job->totalSubdirs(), 3ULL); // see expected result in listRecursive() above
    QVERIFY(job->totalSize() > 54);
#else
    QCOMPARE(job->totalFiles(), 7ULL); // see expected result in listRecursive() above
    QCOMPARE(job->totalSubdirs(), 4ULL); // see expected result in listRecursive() above
    QVERIFY2(job->totalSize() >= 60, qPrintable(QString("totalSize was %1").arg(job->totalSize()))); // size of subdir entries is filesystem dependent. E.g. this is 16428 with ext4 but only 272 with xfs, and 63 on FreeBSD
#endif

    qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void JobTest::directorySizeError()
{
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(QStringLiteral("/I/Dont/Exist")));
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(!ok);
    qApp->sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void JobTest::slotEntries(KIO::Job *, const KIO::UDSEntryList &lst)
{
    for (KIO::UDSEntryList::ConstIterator it = lst.begin(); it != lst.end(); ++it) {
        QString displayName = (*it).stringValue(KIO::UDSEntry::UDS_NAME);
        //QUrl url = (*it).stringValue( KIO::UDSEntry::UDS_URL );
        m_names.append(displayName);
    }
}

void JobTest::calculateRemainingSeconds()
{
    unsigned int seconds = KIO::calculateRemainingSeconds(2 * 86400 - 60, 0, 1);
    QCOMPARE(seconds, static_cast<unsigned int>(2 * 86400 - 60));
    QString text = KIO::convertSeconds(seconds);
    QCOMPARE(text, i18n("1 day 23:59:00"));

    seconds = KIO::calculateRemainingSeconds(520, 20, 10);
    QCOMPARE(seconds, static_cast<unsigned int>(50));
    text = KIO::convertSeconds(seconds);
    QCOMPARE(text, i18n("00:00:50"));
}

#if 0
void JobTest::copyFileToSystem()
{
    if (!KProtocolInfo::isKnownProtocol("system")) {
        qDebug() << "no kio_system, skipping test";
        return;
    }

    // First test with support for UDS_LOCAL_PATH
    copyFileToSystem(true);

    QString dest = realSystemPath() + "fileFromHome_copied";
    QFile::remove(dest);

    // Then disable support for UDS_LOCAL_PATH, i.e. test what would
    // happen for ftp, smb, http etc.
    copyFileToSystem(false);
}

void JobTest::copyFileToSystem(bool resolve_local_urls)
{
    qDebug() << resolve_local_urls;
    extern KIOCORE_EXPORT bool kio_resolve_local_urls;
    kio_resolve_local_urls = resolve_local_urls;

    const QString src = homeTmpDir() + "fileFromHome";
    createTestFile(src);
    QUrl u = QUrl::fromLocalFile(src);
    QUrl d = QUrl::fromLocalFile(systemTmpDir());
    d.addPath("fileFromHome_copied");

    qDebug() << "copying " << u << " to " << d;

    // copy the file with file_copy
    m_mimetype.clear();
    KIO::FileCopyJob *job = KIO::file_copy(u, d, -1, KIO::HideProgressInfo);
    job->setUiDelegate(0);
    connect(job, SIGNAL(mimetype(KIO::Job*,QString)),
            this, SLOT(slotMimetype(KIO::Job*,QString)));
    bool ok = job->exec();
    QVERIFY(ok);

    QString dest = realSystemPath() + "fileFromHome_copied";

    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there

    {
        // do NOT check that the timestamp is the same.
        // It can't work with file_copy when it uses the datapump,
        // unless we use setModificationTime in the app code.
    }

    // Check mimetype
    QCOMPARE(m_mimetype, QString("text/plain"));

    // cleanup and retry with KIO::copy()
    QFile::remove(dest);
    job = KIO::copy(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(0);
    ok = job->exec();
    QVERIFY(ok);
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there
    {
        // check that the timestamp is the same (#79937)
        QFileInfo srcInfo(src);
        QFileInfo destInfo(dest);
        QCOMPARE(srcInfo.lastModified(), destInfo.lastModified());
    }

    // restore normal behavior
    kio_resolve_local_urls = true;
}
#endif

void JobTest::getInvalidUrl()
{
    QUrl url(QStringLiteral("http://strange<hostname>/"));
    QVERIFY(!url.isValid());

    KIO::SimpleJob *job = KIO::get(url, KIO::NoReload, KIO::HideProgressInfo);
    QVERIFY(job != nullptr);
    job->setUiDelegate(nullptr);

    KIO::Scheduler::setJobPriority(job, 1); // shouldn't crash (#135456)

    bool ok = job->exec();
    QVERIFY(!ok);   // it should fail :)
}

void JobTest::slotMimetype(KIO::Job *job, const QString &type)
{
    QVERIFY(job != nullptr);
    m_mimetype = type;
}

void JobTest::deleteFile()
{
    const QString dest = otherTmpDir() + "fileFromHome_copied";
    createTestFile(dest);
    KIO::Job *job = KIO::del(QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(dest));
}

void JobTest::deleteDirectory()
{
    const QString dest = otherTmpDir() + "dirFromHome_copied";
    if (!QFile::exists(dest)) {
        createTestDirectory(dest);
    }
    // Let's put a few things in there to see if the recursive deletion works correctly
    // A hidden file:
    createTestFile(dest + "/.hidden");
#ifndef Q_OS_WIN
    // A broken symlink:
    createTestSymlink(dest + "/broken_symlink");
    // A symlink to a dir:
    bool symlink_ok = symlink(QFile::encodeName(QFileInfo(QFINDTESTDATA("jobtest.cpp")).absolutePath()).constData(),
                              QFile::encodeName(dest + "/symlink_to_dir").constData()) == 0;
    if (!symlink_ok) {
        qFatal("couldn't create symlink: %s", strerror(errno));
    }
#endif

    KIO::Job *job = KIO::del(QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(dest));
}

void JobTest::deleteSymlink(bool using_fast_path)
{
    extern KIOCORE_EXPORT bool kio_resolve_local_urls;
    kio_resolve_local_urls = !using_fast_path;

#ifndef Q_OS_WIN
    const QString src = homeTmpDir() + "dirFromHome";
    createTestDirectory(src);
    QVERIFY(QFile::exists(src));
    const QString dest = homeTmpDir() + "/dirFromHome_link";
    if (!QFile::exists(dest)) {
        // Add a symlink to a dir, to make sure we don't recurse into those
        bool symlinkOk = symlink(QFile::encodeName(src).constData(), QFile::encodeName(dest).constData()) == 0;
        QVERIFY(symlinkOk);
        QVERIFY(QFile::exists(dest));
    }
    KIO::Job *job = KIO::del(QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(dest));
    QVERIFY(QFile::exists(src));
#endif

    kio_resolve_local_urls = true;
}

void JobTest::deleteSymlink()
{
#ifndef Q_OS_WIN
    deleteSymlink(true);
    deleteSymlink(false);
#endif
}

void JobTest::deleteManyDirs(bool using_fast_path)
{
    extern KIOCORE_EXPORT bool kio_resolve_local_urls;
    kio_resolve_local_urls = !using_fast_path;

    const int numDirs = 50;
    QList<QUrl> dirs;
    for (int i = 0; i < numDirs; ++i) {
        const QString dir = homeTmpDir() + "dir" + QString::number(i);
        createTestDirectory(dir);
        dirs << QUrl::fromLocalFile(dir);
    }
    QTime dt;
    dt.start();
    KIO::Job *job = KIO::del(dirs, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    Q_FOREACH (const QUrl &dir, dirs) {
        QVERIFY(!QFile::exists(dir.toLocalFile()));
    }

    qDebug() << "Deleted" << numDirs << "dirs in" << dt.elapsed() << "milliseconds";
    kio_resolve_local_urls = true;
}

void JobTest::deleteManyDirs()
{
    deleteManyDirs(true);
    deleteManyDirs(false);
}

static QList<QUrl> createManyFiles(const QString &baseDir, int numFiles)
{
    QList<QUrl> ret;
    ret.reserve(numFiles);
    for (int i = 0; i < numFiles; ++i) {
        // create empty file
        const QString file = baseDir + QString::number(i);
        QFile f(file);
        bool ok = f.open(QIODevice::WriteOnly);
        if (ok) {
            f.write("Hello");
            ret.append(QUrl::fromLocalFile(file));
        }
    }
    return ret;
}

void JobTest::deleteManyFilesIndependently()
{
    QTime dt;
    dt.start();
    const int numFiles = 100; // Use 1000 for performance testing
    const QString baseDir = homeTmpDir();
    const QList<QUrl> urls = createManyFiles(baseDir, numFiles);
    QCOMPARE(urls.count(), numFiles);
    for (int i = 0; i < numFiles; ++i) {
        // delete each file independently. lots of jobs. this stress-tests kio scheduling.
        const QUrl url = urls.at(i);
        const QString file = url.toLocalFile();
        QVERIFY(QFile::exists(file));
        //qDebug() << file;
        KIO::Job *job = KIO::del(url, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        bool ok = job->exec();
        QVERIFY(ok);
        QVERIFY(!QFile::exists(file));
    }
    qDebug() << "Deleted" << numFiles << "files in" << dt.elapsed() << "milliseconds";
}

void JobTest::deleteManyFilesTogether(bool using_fast_path)
{
    extern KIOCORE_EXPORT bool kio_resolve_local_urls;
    kio_resolve_local_urls = !using_fast_path;

    QTime dt;
    dt.start();
    const int numFiles = 100; // Use 1000 for performance testing
    const QString baseDir = homeTmpDir();
    const QList<QUrl> urls = createManyFiles(baseDir, numFiles);
    QCOMPARE(urls.count(), numFiles);

    //qDebug() << file;
    KIO::Job *job = KIO::del(urls, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    qDebug() << "Deleted" << numFiles << "files in" << dt.elapsed() << "milliseconds";

    kio_resolve_local_urls = true;
}

void JobTest::deleteManyFilesTogether()
{
    deleteManyFilesTogether(true);
    deleteManyFilesTogether(false);
}

void JobTest::rmdirEmpty()
{
    const QString dir = homeTmpDir() + "dir";
    QDir().mkdir(dir);
    QVERIFY(QFile::exists(dir));
    KIO::Job *job = KIO::rmdir(QUrl::fromLocalFile(dir));
    QVERIFY(job->exec());
    QVERIFY(!QFile::exists(dir));
}

void JobTest::rmdirNotEmpty()
{
    const QString dir = homeTmpDir() + "dir";
    createTestDirectory(dir);
    createTestDirectory(dir + "/subdir");
    KIO::Job *job = KIO::rmdir(QUrl::fromLocalFile(dir));
    QVERIFY(!job->exec());
    QVERIFY(QFile::exists(dir));
}

void JobTest::stat()
{
#if 1
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    const QUrl url(QUrl::fromLocalFile(filePath));
    KIO::StatJob *job = KIO::stat(url, KIO::HideProgressInfo);
    QVERIFY(job);
    bool ok = job->exec();
    QVERIFY(ok);
    // TODO set setSide, setDetails
    const KIO::UDSEntry &entry = job->statResult();
    QVERIFY(!entry.isDir());
    QVERIFY(!entry.isLink());
    QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("fileFromHome"));

    // Compare what we get via kio_file and what we get when KFileItem stat()s directly
    const KFileItem kioItem(entry, url);
    const KFileItem fileItem(url);
    QCOMPARE(kioItem.name(), fileItem.name());
    QCOMPARE(kioItem.url(), fileItem.url());
    QCOMPARE(kioItem.size(), fileItem.size());
    QCOMPARE(kioItem.user(), fileItem.user());
    QCOMPARE(kioItem.group(), fileItem.group());
    QCOMPARE(kioItem.mimetype(), fileItem.mimetype());
    QCOMPARE(kioItem.permissions(), fileItem.permissions());
    QCOMPARE(kioItem.time(KFileItem::ModificationTime), fileItem.time(KFileItem::ModificationTime));
    QCOMPARE(kioItem.time(KFileItem::AccessTime), fileItem.time(KFileItem::AccessTime));

#else
    // Testing stat over HTTP
    KIO::StatJob *job = KIO::stat(QUrl("http://www.kde.org"), KIO::HideProgressInfo);
    QVERIFY(job);
    bool ok = job->exec();
    QVERIFY(ok);
    // TODO set setSide, setDetails
    const KIO::UDSEntry &entry = job->statResult();
    QVERIFY(!entry.isDir());
    QVERIFY(!entry.isLink());
    QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_NAME), QString());
#endif
}

#ifndef Q_OS_WIN
void JobTest::statSymlink()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    const QString symlink = otherTmpDir() + "link";
    QVERIFY(QFile(filePath).link(symlink));
    QVERIFY(QFile::exists(symlink));
    setTimeStamp(symlink, QDateTime::currentDateTime().addSecs(-20)); // differentiate link time and source file time

    const QUrl url(QUrl::fromLocalFile(symlink));
    KIO::StatJob *job = KIO::stat(url, KIO::HideProgressInfo);
    QVERIFY(job);
    bool ok = job->exec();
    QVERIFY(ok);
    // TODO set setSide, setDetails
    const KIO::UDSEntry &entry = job->statResult();
    QVERIFY(!entry.isDir());
    QVERIFY(entry.isLink());
    QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("link"));

    // Compare what we get via kio_file and what we get when KFileItem stat()s directly
    const KFileItem kioItem(entry, url);
    const KFileItem fileItem(url);
    QCOMPARE(kioItem.name(), fileItem.name());
    QCOMPARE(kioItem.url(), fileItem.url());
    QVERIFY(kioItem.isLink());
    QVERIFY(fileItem.isLink());
    QCOMPARE(kioItem.linkDest(), fileItem.linkDest());
    QCOMPARE(kioItem.size(), fileItem.size());
    QCOMPARE(kioItem.user(), fileItem.user());
    QCOMPARE(kioItem.group(), fileItem.group());
    QCOMPARE(kioItem.mimetype(), fileItem.mimetype());
    QCOMPARE(kioItem.permissions(), fileItem.permissions());
    QCOMPARE(kioItem.time(KFileItem::ModificationTime), fileItem.time(KFileItem::ModificationTime));
    QCOMPARE(kioItem.time(KFileItem::AccessTime), fileItem.time(KFileItem::AccessTime));
}
#endif

void JobTest::mostLocalUrl()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    KIO::StatJob *job = KIO::mostLocalUrl(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo);
    QVERIFY(job);
    bool ok = job->exec();
    QVERIFY(ok);
    QCOMPARE(job->mostLocalUrl().toLocalFile(), filePath);
}

void JobTest::chmodFile()
{
    const QString filePath = homeTmpDir() + "fileForChmod";
    createTestFile(filePath);
    KFileItem item(QUrl::fromLocalFile(filePath));
    const mode_t origPerm = item.permissions();
    mode_t newPerm = origPerm ^ S_IWGRP;
    QVERIFY(newPerm != origPerm);
    KFileItemList items; items << item;
    KIO::Job *job = KIO::chmod(items, newPerm, S_IWGRP /*TODO: QFile::WriteGroup*/, QString(), QString(), false, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());

    KFileItem newItem(QUrl::fromLocalFile(filePath));
    QCOMPARE(QString::number(newItem.permissions(), 8), QString::number(newPerm, 8));
    QFile::remove(filePath);
}

#ifdef Q_OS_UNIX
void JobTest::chmodSticky()
{
    const QString dirPath = homeTmpDir() + "dirForChmodSticky";
    QDir().mkpath(dirPath);
    KFileItem item(QUrl::fromLocalFile(dirPath));
    const mode_t origPerm = item.permissions();
    mode_t newPerm = origPerm ^ S_ISVTX;
    QVERIFY(newPerm != origPerm);
    KFileItemList items({item});
    KIO::Job *job = KIO::chmod(items, newPerm, S_ISVTX, QString(), QString(), false, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());

    KFileItem newItem(QUrl::fromLocalFile(dirPath));
    QCOMPARE(QString::number(newItem.permissions(), 8), QString::number(newPerm, 8));
    QVERIFY(QDir().rmdir(dirPath));
}
#endif

void JobTest::chmodFileError()
{
    // chown(root) should fail
    const QString filePath = homeTmpDir() + "fileForChmod";
    createTestFile(filePath);
    KFileItem item(QUrl::fromLocalFile(filePath));
    const mode_t origPerm = item.permissions();
    mode_t newPerm = origPerm ^ S_IWGRP;
    QVERIFY(newPerm != origPerm);
    KFileItemList items; items << item;
    KIO::Job *job = KIO::chmod(items, newPerm, S_IWGRP /*TODO: QFile::WriteGroup*/, QStringLiteral("root"), QString(), false, KIO::HideProgressInfo);
    // Simulate the user pressing "Skip" in the dialog.
    PredefinedAnswerJobUiDelegate extension;
    extension.m_skipResult = KIO::S_SKIP;
    job->setUiDelegateExtension(&extension);

    QVERIFY(job->exec());

    QCOMPARE(extension.m_askSkipCalled, 1);
    KFileItem newItem(QUrl::fromLocalFile(filePath));
    // We skipped, so the chmod didn't happen.
    QCOMPARE(QString::number(newItem.permissions(), 8), QString::number(origPerm, 8));
    QFile::remove(filePath);
}

void JobTest::mimeType()
{
#if 1
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    KIO::MimetypeJob *job = KIO::mimetype(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo);
    QVERIFY(job);
    QSignalSpy spyMimeType(job, SIGNAL(mimetype(KIO::Job*,QString)));
    bool ok = job->exec();
    QVERIFY(ok);
    QCOMPARE(spyMimeType.count(), 1);
    QCOMPARE(spyMimeType[0][0], QVariant::fromValue(static_cast<KIO::Job *>(job)));
    QCOMPARE(spyMimeType[0][1].toString(), QStringLiteral("application/octet-stream"));
#else
    // Testing mimetype over HTTP
    KIO::MimetypeJob *job = KIO::mimetype(QUrl("http://www.kde.org"), KIO::HideProgressInfo);
    QVERIFY(job);
    QSignalSpy spyMimeType(job, SIGNAL(mimetype(KIO::Job*,QString)));
    bool ok = job->exec();
    QVERIFY(ok);
    QCOMPARE(spyMimeType.count(), 1);
    QCOMPARE(spyMimeType[0][0], QVariant::fromValue(static_cast<KIO::Job *>(job)));
    QCOMPARE(spyMimeType[0][1].toString(), QString("text/html"));
#endif
}

void JobTest::mimeTypeError()
{
    // KIO::mimetype() on a file that doesn't exist
    const QString filePath = homeTmpDir() + "doesNotExist";
    KIO::MimetypeJob *job = KIO::mimetype(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo);
    QVERIFY(job);
    QSignalSpy spyMimeType(job, SIGNAL(mimetype(KIO::Job*,QString)));
    QSignalSpy spyResult(job, SIGNAL(result(KJob*)));
    bool ok = job->exec();
    QVERIFY(!ok);
    QCOMPARE(spyMimeType.count(), 0);
    QCOMPARE(spyResult.count(), 1);
}

void JobTest::moveFileDestAlreadyExists() // #157601
{
    const QString file1 = homeTmpDir() + "fileFromHome";
    createTestFile(file1);
    const QString file2 = homeTmpDir() + "anotherFile";
    createTestFile(file2);
    const QString existingDest = otherTmpDir() + "fileFromHome";
    createTestFile(existingDest);

    QList<QUrl> urls; urls << QUrl::fromLocalFile(file1) << QUrl::fromLocalFile(file2);
    KIO::CopyJob *job = KIO::move(urls, QUrl::fromLocalFile(otherTmpDir()), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    job->setAutoSkip(true);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(QFile::exists(file1)); // it was skipped
    QVERIFY(!QFile::exists(file2)); // it was moved
}

void JobTest::moveDestAlreadyExistsAutoRename_data()
{
    QTest::addColumn<bool>("samePartition");
    QTest::addColumn<bool>("moveDirs");

    QTest::newRow("files same partition") << true << false;
    QTest::newRow("files other partition") << false << false;
    QTest::newRow("dirs same partition") << true << true;
    QTest::newRow("dirs other partition") << false << true;
}

void JobTest::moveDestAlreadyExistsAutoRename()
{
    QFETCH(bool, samePartition);
    QFETCH(bool, moveDirs);

    QString dir;
    if (samePartition) {
        dir = homeTmpDir() + "dir/";
        QVERIFY(QDir(dir).exists() || QDir().mkdir(dir));
    } else {
        dir = otherTmpDir();
    }
    moveDestAlreadyExistsAutoRename(dir, moveDirs);

    if (samePartition) {
        // cleanup
        KIO::Job *job = KIO::del(QUrl::fromLocalFile(dir), KIO::HideProgressInfo);
        QVERIFY(job->exec());
        QVERIFY(!QFile::exists(dir));
    }
}

void JobTest::moveDestAlreadyExistsAutoRename(const QString &destDir, bool moveDirs) // #256650
{
    const QString prefix = moveDirs ? QStringLiteral("dir ") : QStringLiteral("file ");
    QStringList sources;
    const QString file1 = homeTmpDir() + prefix + "(1)";
    const QString file2 = homeTmpDir() + prefix + "(2)";
    const QString existingDest1 = destDir + prefix + "(1)";
    const QString existingDest2 = destDir + prefix + "(2)";
    sources << file1 << file2 << existingDest1 << existingDest2;
    Q_FOREACH (const QString &source, sources) {
        if (moveDirs) {
            QVERIFY(QDir().mkdir(source));
        } else {
            createTestFile(source);
        }
    }

    QList<QUrl> urls; urls << QUrl::fromLocalFile(file1) << QUrl::fromLocalFile(file2);
    KIO::CopyJob *job = KIO::move(urls, QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    job->setAutoRename(true);

    //qDebug() << QDir(destDir).entryList();

    bool ok = job->exec();

    qDebug() << QDir(destDir).entryList();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(file1)); // it was moved
    QVERIFY(!QFile::exists(file2)); // it was moved
    QVERIFY(QFile::exists(existingDest1));
    QVERIFY(QFile::exists(existingDest2));
    const QString file3 = destDir + prefix + "(3)";
    const QString file4 = destDir + prefix + "(4)";
    QVERIFY(QFile::exists(file3));
    QVERIFY(QFile::exists(file4));
    if (moveDirs) {
        QDir().rmdir(file1);
        QDir().rmdir(file2);
        QDir().rmdir(file3);
        QDir().rmdir(file4);
    } else {
        QFile::remove(file1);
        QFile::remove(file2);
        QFile::remove(file3);
        QFile::remove(file4);
    }
}

void JobTest::safeOverwrite_data()
{
    QTest::addColumn<bool>("destFileExists");

    QTest::newRow("dest file exists") << true;
    QTest::newRow("dest file doesn't exist") << false;
}

void JobTest::safeOverwrite()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif

    QFETCH(bool, destFileExists);
    const QString srcDir = homeTmpDir() + "overwrite";
    const QString srcFile = srcDir + "/testfile";
    const QString destDir = otherTmpDir() + "overwrite_other";
    const QString destFile = destDir + "/testfile";
    const QString destPartFile = destFile + ".part";

    createTestDirectory(srcDir);
    createTestDirectory(destDir);

    QVERIFY(QFile::resize(srcFile, 1000000)); //~1MB
    if (!destFileExists) {
        QVERIFY(QFile::remove(destFile));
    }

    KIO::FileCopyJob *job = KIO::file_move(QUrl::fromLocalFile(srcFile), QUrl::fromLocalFile(destFile), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    QSignalSpy spyTotalSize(job, &KIO::Job::totalSize);
    connect(job, &KIO::Job::totalSize, this, [destFileExists, destPartFile](KJob *job, qulonglong totalSize) {
        Q_UNUSED(job);
        Q_UNUSED(totalSize);
        QCOMPARE(destFileExists,  QFile::exists(destPartFile));
    });
    QVERIFY(job->exec());
    QVERIFY(QFile::exists(destFile));
    QVERIFY(!QFile::exists(srcFile));
    QVERIFY(!QFile::exists(destPartFile));
    QCOMPARE(spyTotalSize.count(), 1);

    QDir(srcDir).removeRecursively();
    QDir(destDir).removeRecursively();
}

void JobTest::moveAndOverwrite()
{
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    QString existingDest = otherTmpDir() + "fileFromHome";
    createTestFile(existingDest);

    KIO::FileCopyJob *job = KIO::file_move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    bool ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(sourceFile)); // it was moved

#ifndef Q_OS_WIN
    // Now same thing when the target is a symlink to the source
    createTestFile(sourceFile);
    createTestSymlink(existingDest, QFile::encodeName(sourceFile));
    QVERIFY(QFile::exists(existingDest));
    job = KIO::file_move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(sourceFile)); // it was moved

    // Now same thing when the target is a symlink to another file
    createTestFile(sourceFile);
    createTestFile(sourceFile + "2");
    createTestSymlink(existingDest, QFile::encodeName(sourceFile + "2"));
    QVERIFY(QFile::exists(existingDest));
    job = KIO::file_move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(sourceFile)); // it was moved

    // Now same thing when the target is a _broken_ symlink
    createTestFile(sourceFile);
    createTestSymlink(existingDest);
    QVERIFY(!QFile::exists(existingDest)); // it exists, but it's broken...
    job = KIO::file_move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    ok = job->exec();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(sourceFile)); // it was moved
#endif
}

void JobTest::moveOverSymlinkToSelf() // #169547
{
#ifndef Q_OS_WIN
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    const QString existingDest = homeTmpDir() + "testlink";
    createTestSymlink(existingDest, QFile::encodeName(sourceFile));
    QVERIFY(QFile::exists(existingDest));

    KIO::CopyJob *job = KIO::move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    bool ok = job->exec();
    QVERIFY(!ok);
    QCOMPARE(job->error(), (int)KIO::ERR_FILE_ALREADY_EXIST); // and not ERR_IDENTICAL_FILES!
    QVERIFY(QFile::exists(sourceFile)); // it not moved
#endif
}

void JobTest::createSymlink()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    const QString destDir = homeTmpDir() + "dest";
    QVERIFY(QDir().mkpath(destDir));

    // With KIO::link (high-level)
    KIO::CopyJob *job = KIO::link(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    QVERIFY(job->exec());
    QVERIFY(QFileInfo::exists(sourceFile));
    const QString dest = destDir + "/fileFromHome";
    QVERIFY(QFileInfo(dest).isSymLink());
    QCOMPARE(QFileInfo(dest).symLinkTarget(), sourceFile);
    QFile::remove(dest);

    // With KIO::symlink (low-level)
    const QString linkPath = destDir + "/link";
    KIO::Job *symlinkJob = KIO::symlink(sourceFile, QUrl::fromLocalFile(linkPath), KIO::HideProgressInfo);
    QVERIFY(symlinkJob->exec());
    QVERIFY(QFileInfo::exists(sourceFile));
    QVERIFY(QFileInfo(linkPath).isSymLink());
    QCOMPARE(QFileInfo(linkPath).symLinkTarget(), sourceFile);

    // Cleanup
    QVERIFY(QDir(destDir).removeRecursively());
}

void JobTest::createSymlinkTargetDirDoesntExist()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    const QString destDir = homeTmpDir() + "dest/does/not/exist";

    KIO::CopyJob *job = KIO::link(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), static_cast<int>(KIO::ERR_CANNOT_SYMLINK));
}

void JobTest::createSymlinkAsShouldSucceed()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    const QString dest = homeTmpDir() + "testlink";
    QFile::remove(dest); // just in case

    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(job->exec());
    QVERIFY(QFileInfo::exists(sourceFile));
    QVERIFY(QFileInfo(dest).isSymLink());
    QVERIFY(QFile::remove(dest));
}

void JobTest::createSymlinkAsShouldFailDirectoryExists()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    const QString dest = homeTmpDir() + "dest";
    QVERIFY(QDir().mkpath(dest)); // dest exists as a directory

    // With KIO::link (high-level)
    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_DIR_ALREADY_EXIST);
    QVERIFY(QFileInfo::exists(sourceFile));
    QVERIFY(!QFileInfo::exists(dest + "/fileFromHome"));

    // With KIO::symlink (low-level)
    KIO::Job *symlinkJob = KIO::symlink(sourceFile, QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(!symlinkJob->exec());
    QCOMPARE(symlinkJob->error(), (int)KIO::ERR_DIR_ALREADY_EXIST);
    QVERIFY(QFileInfo::exists(sourceFile));

    // Cleanup
    QVERIFY(QDir().rmdir(dest));
}

void JobTest::createSymlinkAsShouldFailFileExists()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    const QString dest = homeTmpDir() + "testlink";
    QFile::remove(dest); // just in case

    // First time works
    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(job->exec());
    QVERIFY(QFileInfo(dest).isSymLink());

    // Second time fails (already exists)
    job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_FILE_ALREADY_EXIST);

    // KIO::symlink fails too
    KIO::Job *symlinkJob = KIO::symlink(sourceFile, QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(!symlinkJob->exec());
    QCOMPARE(symlinkJob->error(), (int)KIO::ERR_FILE_ALREADY_EXIST);

    // Cleanup
    QVERIFY(QFile::remove(sourceFile));
    QVERIFY(QFile::remove(dest));
}

void JobTest::createSymlinkWithOverwriteShouldWork()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    const QString sourceFile = homeTmpDir() + "fileFromHome";
    createTestFile(sourceFile);
    const QString dest = homeTmpDir() + "testlink";
    QFile::remove(dest); // just in case

    // First time works
    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(job->exec());
    QVERIFY(QFileInfo(dest).isSymLink());

    // Changing the link target, with overwrite, works
    job = KIO::linkAs(QUrl::fromLocalFile(sourceFile + "2"), QUrl::fromLocalFile(dest), KIO::Overwrite | KIO::HideProgressInfo);
    QVERIFY(job->exec());
    QVERIFY(QFileInfo(dest).isSymLink());
    QCOMPARE(QFileInfo(dest).symLinkTarget(), QString(sourceFile + "2"));

    // Changing the link target using KIO::symlink, with overwrite, works
    KIO::Job *symlinkJob = KIO::symlink(sourceFile + "3", QUrl::fromLocalFile(dest), KIO::Overwrite | KIO::HideProgressInfo);
    QVERIFY(symlinkJob->exec());
    QVERIFY(QFileInfo(dest).isSymLink());
    QCOMPARE(QFileInfo(dest).symLinkTarget(), QString(sourceFile + "3"));

    // Cleanup
    QVERIFY(QFile::remove(dest));
    QVERIFY(QFile::remove(sourceFile));
}

void JobTest::createBrokenSymlink()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    const QString sourceFile = "/does/not/exist";
    const QString dest = homeTmpDir() + "testlink";
    QFile::remove(dest); // just in case
    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(job->exec());
    QVERIFY(QFileInfo(dest).isSymLink());

    // Second time fails (already exists)
    job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_FILE_ALREADY_EXIST);
    QVERIFY(QFile::remove(dest));
}

void JobTest::multiGet()
{
    const int numFiles = 10;
    const QString baseDir = homeTmpDir();
    const QList<QUrl> urls = createManyFiles(baseDir, numFiles);
    QCOMPARE(urls.count(), numFiles);

    //qDebug() << file;
    KIO::MultiGetJob *job = KIO::multi_get(0, urls.at(0), KIO::MetaData()); // TODO: missing KIO::HideProgressInfo
    QSignalSpy spyData(job, SIGNAL(data(long,QByteArray)));
    QSignalSpy spyMimeType(job, SIGNAL(mimetype(long,QString)));
    QSignalSpy spyResultId(job, SIGNAL(result(long)));
    QSignalSpy spyResult(job, SIGNAL(result(KJob*)));
    job->setUiDelegate(nullptr);

    for (int i = 1; i < numFiles; ++i) {
        const QUrl url = urls.at(i);
        job->get(i, url, KIO::MetaData());
    }
    //connect(job, &KIO::MultiGetJob::result, [=] (long id) { qDebug() << "ID I got" << id;});
    //connect(job, &KJob::result, [this](KJob* ) {qDebug() << "END";});

    bool ok = job->exec();
    QVERIFY(ok);

    QCOMPARE(spyResult.count(), 1);
    QCOMPARE(spyResultId.count(), numFiles);
    QCOMPARE(spyMimeType.count(), numFiles);
    QCOMPARE(spyData.count(), numFiles * 2);
    for (int i = 0; i < numFiles; ++i) {
        QCOMPARE(spyResultId.at(i).at(0).toInt(), i);
        QCOMPARE(spyMimeType.at(i).at(0).toInt(), i);
        QCOMPARE(spyMimeType.at(i).at(1).toString(), QStringLiteral("text/plain"));
        QCOMPARE(spyData.at(i * 2).at(0).toInt(), i);
        QCOMPARE(QString(spyData.at(i * 2).at(1).toByteArray()), QStringLiteral("Hello"));
        QCOMPARE(spyData.at(i * 2 + 1).at(0).toInt(), i);
        QCOMPARE(QString(spyData.at(i * 2 + 1).at(1).toByteArray()), QLatin1String(""));
    }
}

