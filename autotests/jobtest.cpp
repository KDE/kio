/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "jobtest.h"

#include <KLocalizedString>
#include <KJobUiDelegate>

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
#include <QElapsedTimer>
#include <QProcess>

#include <kmountpoint.h>
#include <kprotocolinfo.h>
#include <kio/scheduler.h>
#include <kio/directorysizejob.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/chmodjob.h>
#include <kio/statjob.h>
#include "kiotesthelper.h" // createTestFile etc.
#ifndef Q_OS_WIN
#include <unistd.h> // for readlink
#endif
#include "mockcoredelegateextensions.h"

QTEST_MAIN(JobTest)

// The code comes partly from kdebase/kioslave/trash/testtrash.cpp

static QString otherTmpDir()
{
#ifdef Q_OS_WIN
    return QDir::tempPath() + "/jobtest/";
#else
    // This one needs to be on another partition, but we can't guarantee that it is
    // On CI, it typically isn't...
    return QStringLiteral("/tmp/jobtest/");
#endif
}

static bool otherTmpDirIsOnSamePartition() // true on CI because it's a LXC container
{
    KMountPoint::Ptr srcMountPoint = KMountPoint::currentMountPoints().findByPath(homeTmpDir());
    KMountPoint::Ptr destMountPoint = KMountPoint::currentMountPoints().findByPath(otherTmpDir());
    Q_ASSERT(srcMountPoint);
    Q_ASSERT(destMountPoint);
    return srcMountPoint->mountedFrom() == destMountPoint->mountedFrom();
}

void JobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::instance()->setApplicationName("kio/jobtest"); // testing for #357499

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");

    // to make sure io is not too fast
    qputenv("KIOSLAVE_FILE_ENABLE_TESTMODE", "1");

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

    /*****
     * Set platform xattr related commands.
     * Linux commands: setfattr, getfattr
     * BSD commands: setextattr, getextattr
     * MacOS commands: xattr -w, xattr -p
     ****/
    m_getXattrCmd = QStandardPaths::findExecutable("getfattr");
    if (m_getXattrCmd.endsWith("getfattr")) {
        m_setXattrCmd = QStandardPaths::findExecutable("setfattr");
        m_setXattrFormatArgs = [](const QString& attrName, const QString& value, const QString& fileName) {
            return QStringList{QLatin1String("-n"), attrName, QLatin1String("-v"), value, fileName};
        };
    } else {
        // On BSD there is lsextattr to list all xattrs and getextattr to get a value
        // for specific xattr. For test purposes lsextattr is more suitable to be used
        // as m_getXattrCmd, so search for it instead of getextattr.
        m_getXattrCmd = QStandardPaths::findExecutable("lsextattr");
        if (m_getXattrCmd.endsWith("lsextattr")) {
            m_setXattrCmd = QStandardPaths::findExecutable("setextattr");
            m_setXattrFormatArgs = [](const QString& attrName, const QString& value, const QString& fileName) {
                return QStringList{QLatin1String("user"), attrName, value, fileName};
            };
        } else {
            m_getXattrCmd = QStandardPaths::findExecutable("xattr");
            m_setXattrFormatArgs = [](const QString& attrName, const QString& value, const QString& fileName) {
                return QStringList{QLatin1String("-w"), attrName, value, fileName};
            };
            if (!m_getXattrCmd.endsWith("xattr")) {
                qWarning() << "Neither getfattr, getextattr nor xattr was found.";
            }
        }
    }

    qRegisterMetaType<KJob *>("KJob*");
    qRegisterMetaType<KIO::Job *>("KIO::Job*");
    qRegisterMetaType<QDateTime>("QDateTime");
}

void JobTest::cleanupTestCase()
{
    QDir(homeTmpDir()).removeRecursively();
    QDir(otherTmpDir()).removeRecursively();
}

struct ScopedCleaner
{
    using Func = std::function<void()>;
    ScopedCleaner(Func f) : m_f(std::move(f)) {}
    ~ScopedCleaner() { m_f(); }
private:
    const Func m_f;
};

void JobTest::enterLoop()
{
    QEventLoop eventLoop;
    connect(this, &JobTest::exitLoop,
            &eventLoop, &QEventLoop::quit);
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

void JobTest::storedGet()
{
    // qDebug();
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    QUrl u = QUrl::fromLocalFile(filePath);
    m_result = -1;

    KIO::StoredTransferJob *job = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, QOverload<KJob *, unsigned long>::of(&KJob::percent));
    QVERIFY(spyPercent.isValid());
    job->setUiDelegate(nullptr);
    connect(job, &KJob::result,
            this, &JobTest::slotGetResult);
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
    Q_EMIT exitLoop();
}

void JobTest::put()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    QUrl u = QUrl::fromLocalFile(filePath);
    KIO::TransferJob *job = KIO::put(u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    job->setModificationTime(mtime);
    job->setUiDelegate(nullptr);
    connect(job, &KJob::result,
            this, &JobTest::slotResult);
    connect(job, &KIO::TransferJob::dataReq,
            this, &JobTest::slotDataReq);
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
    Q_EMIT exitLoop();
}

void JobTest::storedPut()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    QUrl u = QUrl::fromLocalFile(filePath);
    QByteArray putData = "This is the put data";
    KIO::TransferJob *job = KIO::storedPut(putData, u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QSignalSpy spyPercent(job, QOverload<KJob *, unsigned long>::of(&KJob::percent));
    QVERIFY(spyPercent.isValid());
    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    job->setModificationTime(mtime);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QSignalSpy spyPercent(job, QOverload<KJob *, unsigned long>::of(&KJob::percent));
    QVERIFY(spyPercent.isValid());
    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    job->setModificationTime(mtime);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QSignalSpy spyPercent(job, QOverload<KJob *, unsigned long>::of(&KJob::percent));
    QVERIFY(spyPercent.isValid());
    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    job->setModificationTime(mtime);
    job->setTotalSize(putDataContents.size());
    job->setUiDelegate(nullptr);
    job->setAsyncDataEnabled(true);

    // Emit the readChannelFinished even before the job has had time to start
    const auto pos = putDataBuffer.pos();
    int size = putDataBuffer.write(putDataContents);
    putDataBuffer.seek(pos);
    Q_EMIT putDataBuffer.readChannelFinished();

    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QSignalSpy spyPercent(job, QOverload<KJob *, unsigned long>::of(&KJob::percent));
    QVERIFY(spyPercent.isValid());
    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
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
    QTimer::singleShot(450, this, [&putDataBuffer](){ Q_EMIT putDataBuffer.readChannelFinished(); });

    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QSignalSpy spyPercent(job, QOverload<KJob *, unsigned long>::of(&KJob::percent));
    QVERIFY(spyPercent.isValid());
    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
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
    QTimer::singleShot(450, this, [&putDataBuffer](){ Q_EMIT putDataBuffer.readChannelFinished(); });

    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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

    connect(job, &KJob::finished, this, [&jobFinished, &putDataBuffer] {
        putDataBuffer.readyRead();
        jobFinished = true;
    });

    QTimer::singleShot(200, this, [job]() {
        job->kill();
    });

    QTRY_VERIFY(jobFinished);
}

static QHash<QString, QString> getSampleXattrs()
{
    QHash<QString, QString> attrs;
    attrs["user.name with space"] = "value with spaces";
    attrs["user.baloo.rating"] = "1";
    attrs["user.fnewLine"] = "line1\\nline2";
    attrs["user.flistNull"] = "item1\\0item2";
    attrs["user.fattr.with.a.lot.of.namespaces"] = "true";
    attrs["user.fempty"] = "";
    return attrs;
}

bool JobTest::checkXattrFsSupport(const QString &dir)
{
    const QString writeTest = dir + "/fsXattrTestFile";
    createTestFile(writeTest);
    bool ret = setXattr(writeTest);
    QFile::remove(writeTest);
    return ret;
}

bool JobTest::setXattr(const QString &dest)
{
    QProcess xattrWriter;
    xattrWriter.setProcessChannelMode(QProcess::MergedChannels);

    QHash<QString, QString> attrs = getSampleXattrs();
    QHashIterator<QString, QString> i(attrs);
    while (i.hasNext()) {
        i.next();
        QStringList arguments = m_setXattrFormatArgs(i.key(), i.value(), dest);
        xattrWriter.start(m_setXattrCmd, arguments);
        xattrWriter.waitForStarted();
        xattrWriter.waitForFinished(-1);
        if(xattrWriter.exitStatus() != QProcess::NormalExit) {
            return false;
        }
        QList<QByteArray> resultdest = xattrWriter.readAllStandardOutput().split('\n');
        if (!resultdest[0].isEmpty()) {
            QWARN("Error writing user xattr. Xattr copy tests will be disabled.");
            qDebug() << resultdest;
            return false;
        }
    }

    return true;
}

QList<QByteArray> JobTest::readXattr(const QString &src)
{
    QProcess xattrReader;
    xattrReader.setProcessChannelMode(QProcess::MergedChannels);

    QStringList arguments;
    char outputSeparator = '\n';
    // Linux
    if (m_getXattrCmd.endsWith("getfattr")) {
        arguments = QStringList {"-d", src};
    }
    // BSD
    else if (m_getXattrCmd.endsWith("lsextattr")) {
        arguments = QStringList {"-q", "user", src};
        outputSeparator = '\t';
    }
    // MacOS
    else {
        arguments = QStringList {"-l", src };
    }

    xattrReader.start(m_getXattrCmd, arguments);
    xattrReader.waitForFinished();
    QList<QByteArray> result = xattrReader.readAllStandardOutput().split(outputSeparator);
    if (m_getXattrCmd.endsWith("getfattr")) {
        // Line 1 is the file name
        result.removeAt(1);
    }
    else if (m_getXattrCmd.endsWith("lsextattr")) {
        // cut off trailing \n
        result.last().chop(1);
        // lsextattr does not sort its output
        std::sort(result.begin(), result.end());
    }

    return result;
}

void JobTest::compareXattr(const QString &src, const QString &dest)
{
    auto srcAttrs = readXattr(src);
    auto dstAttrs = readXattr(dest);
    QCOMPARE(dstAttrs, srcAttrs);
}

void JobTest::copyLocalFile(const QString &src, const QString &dest)
{
    const QUrl u = QUrl::fromLocalFile(src);
    const QUrl d = QUrl::fromLocalFile(dest);

    const int perms = 0666;
    // copy the file with file_copy
    KIO::Job *job = KIO::file_copy(u, d, perms, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there
    QCOMPARE(int(QFileInfo(dest).permissions()), int(0x6666));
    compareXattr(src, dest);

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
    auto *copyjob = KIO::copy(u, d, KIO::HideProgressInfo);
    QSignalSpy spyCopyingDone(copyjob, &KIO::CopyJob::copyingDone);
    copyjob->setUiDelegate(nullptr);
    copyjob->setUiDelegateExtension(nullptr);
    QVERIFY2(copyjob->exec(), qPrintable(copyjob->errorString()));
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there
    compareXattr(src, dest);
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

    QCOMPARE(copyjob->totalAmount(KJob::Files), 1);
    QCOMPARE(copyjob->totalAmount(KJob::Directories), 0);
    QCOMPARE(copyjob->processedAmount(KJob::Files), 1);
    QCOMPARE(copyjob->processedAmount(KJob::Directories), 0);
    QCOMPARE(copyjob->percent(), 100);

    // cleanup and retry with KIO::copyAs()
    QFile::remove(dest);
    job = KIO::copyAs(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there
    compareXattr(src, dest);

    // Do it again, with Overwrite.
    job = KIO::copyAs(u, d, KIO::Overwrite | KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest));
    QVERIFY(QFile::exists(src));     // still there
    compareXattr(src, dest);

    // Do it again, without Overwrite (should fail).
    job = KIO::copyAs(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(!job->exec());

    // Clean up
    QFile::remove(src);
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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

    QCOMPARE(job->totalAmount(KJob::Files), 2); // testfile and testlink
    QCOMPARE(job->totalAmount(KJob::Directories), 1);
    QCOMPARE(job->processedAmount(KJob::Files), 2);
    QCOMPARE(job->processedAmount(KJob::Directories), 1);
    QCOMPARE(job->percent(), 100);

    // Do it again, with Overwrite.
    // Use copyAs, we don't want a subdir inside d.
    job = KIO::copyAs(u, d, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QCOMPARE(job->totalAmount(KJob::Files), 2); // testfile and testlink
    QCOMPARE(job->totalAmount(KJob::Directories), 1);
    QCOMPARE(job->processedAmount(KJob::Files), 2);
    QCOMPARE(job->processedAmount(KJob::Directories), 1);
    QCOMPARE(job->percent(), 100);

    // Do it again, without Overwrite (should fail).
    job = KIO::copyAs(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(!job->exec());
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
    const QString homeDir = homeTmpDir();
    const QString filePath = homeDir + "fileFromHome";
    const QString dest = homeDir + "fileFromHome_copied";
    createTestFile(filePath);
    if (checkXattrFsSupport(homeDir)) {
        setXattr(filePath);
    }
    copyLocalFile(filePath, dest);
}

void JobTest::copyDirectoryToSamePartition()
{
    // qDebug();
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = homeTmpDir() + "dirFromHome_copied";
    createTestDirectory(src);
    copyLocalDirectory(src, dest);
}

void JobTest::copyDirectoryToExistingDirectory()
{
    // qDebug();
    // just the same as copyDirectoryToSamePartition, but this time dest exists.
    // So we get a subdir, "dirFromHome_copy/dirFromHome"
    const QString src = homeTmpDir() + "dirFromHome";
    const QString dest = homeTmpDir() + "dirFromHome_copied";
    createTestDirectory(src);
    createTestDirectory(dest);
    copyLocalDirectory(src, dest, AlreadyExists);
}

void JobTest::copyDirectoryToExistingSymlinkedDirectory()
{
    // qDebug();
    // just the same as copyDirectoryToSamePartition, but this time dest is a symlink.
    // So we get a file in the symlink dir, "dirFromHome_symlink/dirFromHome" and
    // "dirFromHome_symOrigin/dirFromHome"
    const QString src = homeTmpDir() + "dirFromHome";
    const QString origSymlink = homeTmpDir() + "dirFromHome_symOrigin";
    const QString targetSymlink = homeTmpDir() + "dirFromHome_symlink";
    createTestDirectory(src);
    createTestDirectory(origSymlink);

    bool ok = KIOPrivate::createSymlink(origSymlink, targetSymlink);
    if (!ok) {
        qFatal("couldn't create symlink: %s", strerror(errno));
    }
    QVERIFY(QFileInfo(targetSymlink).isSymLink());
    QVERIFY(QFileInfo(targetSymlink).isDir());

    KIO::Job *job = KIO::copy(QUrl::fromLocalFile(src), QUrl::fromLocalFile(targetSymlink), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(src));     // still there

    // file is visible in both places due to symlink
    QVERIFY(QFileInfo(origSymlink + "/dirFromHome").isDir());;
    QVERIFY(QFileInfo(targetSymlink + "/dirFromHome").isDir());
    QVERIFY(QDir(origSymlink).removeRecursively());
    QVERIFY(QFile::remove(targetSymlink));
}

void JobTest::copyFileToOtherPartition()
{
    // qDebug();
    const QString homeDir = homeTmpDir();
    const QString otherHomeDir = otherTmpDir();
    const QString filePath = homeDir + "fileFromHome";
    const QString dest = otherHomeDir + "fileFromHome_copied";
    bool canRead = checkXattrFsSupport(homeDir);
    bool canWrite = checkXattrFsSupport(otherHomeDir);
    createTestFile(filePath);
    if (canRead && canWrite) {
        setXattr(filePath);
    }
    copyLocalFile(filePath, dest);
}

void JobTest::copyDirectoryToOtherPartition()
{
    // qDebug();
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

    ScopedCleaner cleaner( [&]{
        QFile(inaccessible).setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));

        KIO::DeleteJob *deljob1 = KIO::del(QUrl::fromLocalFile(src_dir), KIO::HideProgressInfo);
        deljob1->setUiDelegate(nullptr); // no skip dialog, thanks
        QVERIFY(deljob1->exec());

        KIO::DeleteJob *deljob2 = KIO::del(QUrl::fromLocalFile(dst_dir), KIO::HideProgressInfo);
        deljob2->setUiDelegate(nullptr); // no skip dialog, thanks
        QVERIFY(deljob2->exec());
    });

    KIO::CopyJob *job = KIO::copy(QUrl::fromLocalFile(src_dir), QUrl::fromLocalFile(dst_dir), KIO::HideProgressInfo);

    QSignalSpy spy(job, &KJob::warning);
    job->setUiDelegate(nullptr);   // no skip dialog, thanks
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QCOMPARE(job->totalAmount(KJob::Files), 4); // testfile, testlink, folder1/testlink, folder1/testfile
    QCOMPARE(job->totalAmount(KJob::Directories), 3); // srcHome, srcHome/folder1, srcHome/folder1/inaccessible
    QCOMPARE(job->processedAmount(KJob::Files), 4);
    QCOMPARE(job->processedAmount(KJob::Directories), 3);
    QCOMPARE(job->percent(), 100);

    QCOMPARE(spy.count(), 1); // one warning should be emitted by the copy job
}

void JobTest::copyDataUrl()
{
    // GIVEN
    const QString dst_dir = homeTmpDir();
    QVERIFY(!QFileInfo::exists(dst_dir + "/data"));
    // WHEN
    KIO::CopyJob *job = KIO::copy(QUrl("data:,Hello%2C%20World!"), QUrl::fromLocalFile(dst_dir), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QSignalSpy spyResult(job, &KJob::result);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->suspend());
    QVERIFY(!spyResult.wait(300));
    QVERIFY(job->resume());
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QSignalSpy spyResult(job, &KJob::result);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->suspend());
    QVERIFY(!spyResult.wait(300));
    QVERIFY(job->resume());
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest));
    QVERIFY(!QFile::exists(src));     // not there anymore
    QCOMPARE(int(QFileInfo(dest).permissions()), int(0x6666));

    // move it back with KIO::move()
    job = KIO::move(d, u, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QT_LSTAT(QFile::encodeName(dest).constData(), &buf) == 0);
    QVERIFY(!QFile::exists(src));     // not there anymore

    // move it back with KIO::move()
    job = KIO::move(d, u, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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

struct CleanupInaccessibleSubdir {
    explicit CleanupInaccessibleSubdir(const QString &subdir) : subdir(subdir) {}
    ~CleanupInaccessibleSubdir() {
        QVERIFY(QFile(subdir).setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)));
        QVERIFY(QDir(subdir).removeRecursively());
    }
private:
    const QString subdir;
};

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
    CleanupInaccessibleSubdir c(subdir);

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

    QCOMPARE(job->totalAmount(KJob::Files), 1);
    QCOMPARE(job->totalAmount(KJob::Directories), 0);
    QCOMPARE(job->processedAmount(KJob::Files), 0);
    QCOMPARE(job->processedAmount(KJob::Directories), 0);
    QCOMPARE(job->percent(), 0);
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
    CleanupInaccessibleSubdir c(subdir);

    // When trying to move it
    const QString dest = homeTmpDir() + "mdnp";
    KIO::CopyJob *job = KIO::move(QUrl::fromLocalFile(src), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr); // no skip dialog, thanks

    // The job should fail with "access denied"
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_ACCESS_DENIED);

    QVERIFY(!QFile::exists(dest));

    QCOMPARE(job->totalAmount(KJob::Files), 1);
    QCOMPARE(job->totalAmount(KJob::Directories), 0);
    QCOMPARE(job->processedAmount(KJob::Files), 0);
    QCOMPARE(job->processedAmount(KJob::Directories), 0);
    QCOMPARE(job->percent(), 0);
}

void JobTest::moveDirectoryToReadonlyFilesystem_data()
{
    QTest::addColumn<QList<QUrl>>("sources");
    QTest::addColumn<int>("expectedErrorCode");

    const QString srcFileHomePath = homeTmpDir() + "srcFileHome";
    const QUrl srcFileHome = QUrl::fromLocalFile(srcFileHomePath);
    createTestFile(srcFileHomePath);

    const QString srcFileOtherPath = otherTmpDir() + "srcFileOther";
    const QUrl srcFileOther = QUrl::fromLocalFile(srcFileOtherPath);
    createTestFile(srcFileOtherPath);

    const QString srcDirHomePath = homeTmpDir() + "srcDirHome";
    const QUrl srcDirHome = QUrl::fromLocalFile(srcDirHomePath);
    createTestDirectory(srcDirHomePath);

    const QString srcDirHome2Path = homeTmpDir() + "srcDirHome2";
    const QUrl srcDirHome2 = QUrl::fromLocalFile(srcDirHome2Path);
    createTestDirectory(srcDirHome2Path);

    const QString srcDirOtherPath = otherTmpDir() + "srcDirOther";
    const QUrl srcDirOther = QUrl::fromLocalFile(srcDirOtherPath);
    createTestDirectory(srcDirOtherPath);

    QTest::newRow("file_same_partition") << QList<QUrl>{srcFileHome} << int(KIO::ERR_WRITE_ACCESS_DENIED);
    QTest::newRow("file_other_partition") << QList<QUrl>{srcFileOther} << int(KIO::ERR_WRITE_ACCESS_DENIED);
    QTest::newRow("one_dir_same_partition") << QList<QUrl>{srcDirHome} << int(KIO::ERR_WRITE_ACCESS_DENIED);
    QTest::newRow("one_dir_other_partition") << QList<QUrl>{srcDirOther} << int(KIO::ERR_WRITE_ACCESS_DENIED);
    QTest::newRow("dirs_same_partition") << QList<QUrl>{srcDirHome, srcDirHome2} << int(KIO::ERR_WRITE_ACCESS_DENIED);
    QTest::newRow("dirs_both_partitions") << QList<QUrl>{srcDirOther, srcDirHome} << int(KIO::ERR_WRITE_ACCESS_DENIED);
}

void JobTest::moveDirectoryToReadonlyFilesystem()
{
    QFETCH(QList<QUrl>, sources);
    QFETCH(int, expectedErrorCode);

    const QString dst_dir = homeTmpDir() + "readonlyDest";
    const QUrl dst = QUrl::fromLocalFile(dst_dir);
    QVERIFY2(QDir().mkdir(dst_dir), qPrintable(dst_dir));
    QFile(dst_dir).setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::ExeOwner)); // Make it readonly, moving should throw some errors

    ScopedCleaner cleaner([&] {
        QVERIFY(QFile(dst_dir).setPermissions(QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner)));
        QVERIFY(QDir(dst_dir).removeRecursively());
    });

    KIO::CopyJob *job = KIO::move(sources, dst, KIO::HideProgressInfo | KIO::NoPrivilegeExecution);
    job->setUiDelegate(nullptr);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), expectedErrorCode);
    for (const QUrl &srcUrl : qAsConst(sources)) {
        QVERIFY(QFileInfo::exists(srcUrl.toLocalFile())); // no moving happened
    }

    KIO::CopyJob *job2 = KIO::move(sources, dst, KIO::HideProgressInfo);
    job2->setUiDelegate(nullptr);
    QVERIFY(!job2->exec());
    if (job2->error() != KIO::ERR_CANNOT_MKDIR) { // This can happen when moving between partitions, but on CI it's the same partition so allow both
        QCOMPARE(job2->error(), expectedErrorCode);
    }
    for (const QUrl &srcUrl : qAsConst(sources)) {
        QVERIFY(QFileInfo::exists(srcUrl.toLocalFile())); // no moving happened
    }
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
    connect(job, &KIO::ListJob::entries,
            this, &JobTest::slotEntries);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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

    const QString joinedNames = m_names.join(QLatin1Char(','));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY(!job->exec());
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

void JobTest::getInvalidUrl()
{
    QUrl url(QStringLiteral("http://strange<hostname>/"));
    QVERIFY(!url.isValid());

    KIO::SimpleJob *job = KIO::get(url, KIO::NoReload, KIO::HideProgressInfo);
    QVERIFY(job != nullptr);
    job->setUiDelegate(nullptr);

    KIO::Scheduler::setJobPriority(job, 1); // shouldn't crash (#135456)

    QVERIFY(!job->exec());   // it should fail :)
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QElapsedTimer dt;
    dt.start();
    KIO::Job *job = KIO::del(dirs, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    for (const QUrl &dir : qAsConst(dirs)) {
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
    QElapsedTimer dt;
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
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QVERIFY(!QFile::exists(file));
    }
    qDebug() << "Deleted" << numFiles << "files in" << dt.elapsed() << "milliseconds";
}

void JobTest::deleteManyFilesTogether(bool using_fast_path)
{
    extern KIOCORE_EXPORT bool kio_resolve_local_urls;
    kio_resolve_local_urls = !using_fast_path;

    QElapsedTimer dt;
    dt.start();
    const int numFiles = 100; // Use 1000 for performance testing
    const QString baseDir = homeTmpDir();
    const QList<QUrl> urls = createManyFiles(baseDir, numFiles);
    QCOMPARE(urls.count(), numFiles);

    //qDebug() << file;
    KIO::Job *job = KIO::del(urls, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    // TODO set setSide
    const KIO::UDSEntry &entry = job->statResult();

    // we only get filename, access, type, size, uid, gid, btime, mtime, atime
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_NAME));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_SIZE));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_FILE_TYPE));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_USER));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_GROUP));
    //QVERIFY(entry.contains(KIO::UDSEntry::UDS_CREATION_TIME)); // only true if st_birthtime or statx is used
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_MODIFICATION_TIME));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS_TIME));
    QCOMPARE(entry.count(), 8 + (entry.contains(KIO::UDSEntry::UDS_CREATION_TIME) ? 1 : 0));

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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    // TODO set setSide, setDetails
    const KIO::UDSEntry &entry = job->statResult();
    QVERIFY(!entry.isDir());
    QVERIFY(!entry.isLink());
    QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_NAME), QString());
#endif
}

void JobTest::statDetailsBasic()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    const QUrl url(QUrl::fromLocalFile(filePath));
    KIO::StatJob *job = KIO::statDetails(url, KIO::StatJob::StatSide::SourceSide, KIO::StatBasic,  KIO::HideProgressInfo);
    QVERIFY(job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    // TODO set setSide
    const KIO::UDSEntry &entry = job->statResult();

    // we only get filename, access, type, size, (no linkdest)
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_NAME));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_SIZE));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_FILE_TYPE));
    QCOMPARE(entry.count(), 4);

    QVERIFY(!entry.isDir());
    QVERIFY(!entry.isLink());
    QVERIFY(entry.numberValue(KIO::UDSEntry::UDS_ACCESS) > 0);
    QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("fileFromHome"));

    // Compare what we get via kio_file and what we get when KFileItem stat()s directly
    // for the requested fields
    const KFileItem kioItem(entry, url);
    const KFileItem fileItem(url);
    QCOMPARE(kioItem.name(), fileItem.name());
    QCOMPARE(kioItem.url(), fileItem.url());
    QCOMPARE(kioItem.size(), fileItem.size());
    QCOMPARE(kioItem.user(), "");
    QCOMPARE(kioItem.group(), "");
    QCOMPARE(kioItem.mimetype(), "application/octet-stream");
    QCOMPARE(kioItem.permissions(), 438);
    QCOMPARE(kioItem.time(KFileItem::ModificationTime), QDateTime());
    QCOMPARE(kioItem.time(KFileItem::AccessTime), QDateTime());
}

void JobTest::statDetailsBasicSetDetails()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    const QUrl url(QUrl::fromLocalFile(filePath));
    KIO::StatJob *job = KIO::stat(url);
    job->setDetails(KIO::StatBasic);
    QVERIFY(job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    // TODO set setSide
    const KIO::UDSEntry &entry = job->statResult();

    // we only get filename, access, type, size, (no linkdest)
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_NAME));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_SIZE));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_FILE_TYPE));
    QCOMPARE(entry.count(), 4);

    QVERIFY(!entry.isDir());
    QVERIFY(!entry.isLink());
    QVERIFY(entry.numberValue(KIO::UDSEntry::UDS_ACCESS) > 0);
    QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("fileFromHome"));

    // Compare what we get via kio_file and what we get when KFileItem stat()s directly
    // for the requested fields
    const KFileItem kioItem(entry, url);
    const KFileItem fileItem(url);
    QCOMPARE(kioItem.name(), fileItem.name());
    QCOMPARE(kioItem.url(), fileItem.url());
    QCOMPARE(kioItem.size(), fileItem.size());
    QCOMPARE(kioItem.user(), "");
    QCOMPARE(kioItem.group(), "");
    QCOMPARE(kioItem.mimetype(), "application/octet-stream");
    QCOMPARE(kioItem.permissions(), 438);
    QCOMPARE(kioItem.time(KFileItem::ModificationTime), QDateTime());
    QCOMPARE(kioItem.time(KFileItem::AccessTime), QDateTime());
}

void JobTest::statWithInode()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    const QUrl url(QUrl::fromLocalFile(filePath));
    KIO::StatJob *job = KIO::statDetails(url, KIO::StatJob::SourceSide, KIO::StatInode);
    QVERIFY(job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    const KIO::UDSEntry entry = job->statResult();
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_DEVICE_ID));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_INODE));
    QCOMPARE(entry.count(), 2);

    const QString path = otherTmpDir() + "otherFile";
    createTestFile(path);
    const QUrl otherUrl(QUrl::fromLocalFile(path));
    KIO::StatJob *otherJob = KIO::statDetails(otherUrl, KIO::StatJob::SourceSide, KIO::StatInode);
    QVERIFY(otherJob);
    QVERIFY2(otherJob->exec(), qPrintable(otherJob->errorString()));

    const KIO::UDSEntry otherEntry = otherJob->statResult();
    QVERIFY(otherEntry.contains(KIO::UDSEntry::UDS_DEVICE_ID));
    QVERIFY(otherEntry.contains(KIO::UDSEntry::UDS_INODE));
    QCOMPARE(otherEntry.count(), 2);

    const int device = entry.numberValue(KIO::UDSEntry::UDS_DEVICE_ID);
    const int otherDevice = otherEntry.numberValue(KIO::UDSEntry::UDS_DEVICE_ID);

    // this test doesn't make sense on the CI as it's an LXC container with one partition
    if (otherTmpDirIsOnSamePartition()) {
        // On the CI where the two tmp dirs are on the only parition available
        // in the LXC container, the device ID's would be identical
        QCOMPARE(device, otherDevice);
    }
    else {
        QVERIFY(device != otherDevice);
    }
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
    KIO::StatJob *job = KIO::statDetails(url, KIO::StatJob::StatSide::SourceSide,
                                         KIO::StatBasic | KIO::StatResolveSymlink | KIO::StatUser | KIO::StatTime,
                                         KIO::HideProgressInfo);
    QVERIFY(job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    // TODO set setSide, setDetails
    const KIO::UDSEntry &entry = job->statResult();

    // we only get filename, access, type, size, linkdest, uid, gid, btime, mtime, atime
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_NAME));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_SIZE));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_FILE_TYPE));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_LINK_DEST));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_USER));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_GROUP));
    //QVERIFY(entry.contains(KIO::UDSEntry::UDS_CREATION_TIME)); // only true if st_birthtime or statx is used
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_MODIFICATION_TIME));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS_TIME));
    QCOMPARE(entry.count(), 9 + (entry.contains(KIO::UDSEntry::UDS_CREATION_TIME) ? 1 : 0));

    QVERIFY(!entry.isDir());
    QVERIFY(entry.isLink());
    QVERIFY(entry.numberValue(KIO::UDSEntry::UDS_ACCESS) > 0);
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

/* Check that the underlying system, and Qt, support
 * millisecond timestamp resolution.
 */
void JobTest::statTimeResolution()
{
    const QString filePath = homeTmpDir() + "statFile";
    const QDateTime early70sDate = QDateTime::fromMSecsSinceEpoch(107780520123L);
    const time_t early70sTime = 107780520; // Seconds for January 6 1973, 12:02

    createTestFile(filePath);

    QFile dest_file(filePath);
    QVERIFY(dest_file.open(QIODevice::ReadOnly));
    #if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
        // with nano secs precision
        struct timespec ut[2];
        ut[0].tv_sec = early70sTime;
        ut[0].tv_nsec = 123000000L; // 123 ms
        ut[1] = ut[0];
        // need to do this with the dest file still opened, or this fails
        QCOMPARE(::futimens(dest_file.handle(), ut), 0);
    #else
        struct timeval ut[2];
        ut[0].tv_sec = early70sTime;
        ut[0].tv_usec = 123000;
        ut[1] = ut[0];
        QCOMPARE(::futimes(dest_file.handle(), ut), 0);
    #endif
    dest_file.close();

    // Check that the modification time is set with millisecond precision
    dest_file.setFileName(filePath);
    QDateTime d = dest_file.fileTime(QFileDevice::FileModificationTime);
    QCOMPARE(d, early70sDate);
    QCOMPARE(d.time().msec(), 123);

    #if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
        QT_STATBUF buff_dest;
        QCOMPARE(QT_STAT(filePath.toLocal8Bit().data(), &buff_dest), 0);
        QCOMPARE(buff_dest.st_mtim.tv_sec, early70sTime);
        QCOMPARE(buff_dest.st_mtim.tv_nsec, 123000000L);
    #endif

    QCOMPARE(QFileInfo(filePath).lastModified(), early70sDate);
}
#endif

void JobTest::mostLocalUrl()
{
    const QString filePath = homeTmpDir() + "fileFromHome";
    createTestFile(filePath);
    KIO::StatJob *job = KIO::mostLocalUrl(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo);
    QVERIFY(job);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->mostLocalUrl().toLocalFile(), filePath);
}

void JobTest::mostLocalUrlHttp() {
    // the url is returned as-is, as an http url can't have a mostLocalUrl
    const QUrl url("http://www.google.com");
    KIO::StatJob *httpStat = KIO::mostLocalUrl(url, KIO::HideProgressInfo);
    QVERIFY(httpStat);
    QVERIFY2(httpStat->exec(), qPrintable(httpStat->errorString()));
    QCOMPARE(httpStat->mostLocalUrl(), url);
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

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
    extension.m_skipResult = KIO::Result_Skip;
    job->setUiDelegateExtension(&extension);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

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
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QSignalSpy spyMimeType(job, QOverload<KIO::Job *, const QString &>::of(&KIO::MimetypeJob::mimetype));
#endif
    QSignalSpy spyMimeTypeFound(job, &KIO::TransferJob::mimeTypeFound);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QCOMPARE(spyMimeType.count(), 1);
    QCOMPARE(spyMimeType[0][0], QVariant::fromValue(static_cast<KIO::Job *>(job)));
    QCOMPARE(spyMimeType[0][1].toString(), QStringLiteral("application/octet-stream"));
#endif
    QCOMPARE(spyMimeTypeFound.count(), 1);
    QCOMPARE(spyMimeTypeFound[0][0], QVariant::fromValue(static_cast<KIO::Job *>(job)));
    QCOMPARE(spyMimeTypeFound[0][1].toString(), QStringLiteral("application/octet-stream"));
#else
    // Testing mimetype over HTTP
    KIO::MimetypeJob *job = KIO::mimetype(QUrl("http://www.kde.org"), KIO::HideProgressInfo);
    QVERIFY(job);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QSignalSpy spyMimeType(job, QOverload<KIO::Job *, const QString &>::of(&KIO::MimetypeJob::mimetype));
#endif
    QSignalSpy spyMimeTypeFound(job, &KIO::TransferJob::mimeTypeFound);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QCOMPARE(spyMimeType.count(), 1);
    QCOMPARE(spyMimeType[0][0], QVariant::fromValue(static_cast<KIO::Job *>(job)));
    QCOMPARE(spyMimeType[0][1].toString(), QString("text/html"));
#endif
    QCOMPARE(spyMimeTypeFound.count(), 1);
    QCOMPARE(spyMimeTypeFound[0][0], QVariant::fromValue(static_cast<KIO::Job *>(job)));
    QCOMPARE(spyMimeTypeFound[0][1].toString(), QString("text/html"));
#endif
}

void JobTest::mimeTypeError()
{
    // KIO::mimetype() on a file that doesn't exist
    const QString filePath = homeTmpDir() + "doesNotExist";
    KIO::MimetypeJob *job = KIO::mimetype(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo);
    QVERIFY(job);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QSignalSpy spyMimeType(job, QOverload<KIO::Job *, const QString &>::of(&KIO::MimetypeJob::mimetype));
#endif
    QSignalSpy spyMimeTypeFound(job, &KIO::TransferJob::mimeTypeFound);
    QSignalSpy spyResult(job, &KJob::result);
    QVERIFY(!job->exec());
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QCOMPARE(spyMimeType.count(), 0);
#endif
    QCOMPARE(spyMimeTypeFound.count(), 0);
    QCOMPARE(spyResult.count(), 1);
}

void JobTest::moveFileDestAlreadyExists_data()
{
    QTest::addColumn<bool>("autoSkip");

    QTest::newRow("autoSkip") << true;
    QTest::newRow("manualSkip") << false;
}

void JobTest::moveFileDestAlreadyExists() // #157601
{
    QFETCH(bool, autoSkip);

    const QString file1 = homeTmpDir() + "fileFromHome";
    createTestFile(file1);
    const QString file2 = homeTmpDir() + "fileFromHome2";
    createTestFile(file2);
    const QString file3 = homeTmpDir() + "anotherFile";
    createTestFile(file3);
    const QString existingDest = otherTmpDir() + "fileFromHome";
    createTestFile(existingDest);
    const QString existingDest2 = otherTmpDir() + "fileFromHome2";
    createTestFile(existingDest2);

    ScopedCleaner cleaner([] {
        QFile::remove(otherTmpDir() + "anotherFile");
    });

    const QList<QUrl> urls{QUrl::fromLocalFile(file1), QUrl::fromLocalFile(file2), QUrl::fromLocalFile(file3)};
    KIO::CopyJob *job = KIO::move(urls, QUrl::fromLocalFile(otherTmpDir()), KIO::HideProgressInfo);
    MockAskUserInterface *askUserHandler = nullptr;
    if (autoSkip) {
        job->setUiDelegate(nullptr);
        job->setAutoSkip(true);
    } else {
        // Simulate the user pressing "Skip" in the dialog.
        job->setUiDelegate(new KJobUiDelegate);
        askUserHandler = new MockAskUserInterface(job->uiDelegate());
        askUserHandler->m_renameResult = KIO::Result_Skip;
    }
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (askUserHandler) {
        QCOMPARE(askUserHandler->m_askUserRenameCalled, 2);
        QCOMPARE(askUserHandler->m_askUserSkipCalled, 0);
    }
    QVERIFY(QFile::exists(file1)); // it was skipped
    QVERIFY(QFile::exists(file2)); // it was skipped
    QVERIFY(!QFile::exists(file3)); // it was moved

    QCOMPARE(job->totalAmount(KJob::Files), 3);
    QCOMPARE(job->totalAmount(KJob::Directories), 0);
    QCOMPARE(job->processedAmount(KJob::Files), 1);
    QCOMPARE(job->processedAmount(KJob::Directories), 0);
    QCOMPARE(job->percent(), 100);
}

void JobTest::copyFileDestAlreadyExists_data()
{
    QTest::addColumn<bool>("autoSkip");

    QTest::newRow("autoSkip") << true;
    QTest::newRow("manualSkip") << false;
}

void JobTest::copyFileDestAlreadyExists() // to test skipping when copying
{
    QFETCH(bool, autoSkip);
    const QString file1 = homeTmpDir() + "fileFromHome";
    createTestFile(file1);
    const QString file2 = homeTmpDir() + "anotherFile";
    createTestFile(file2);
    const QString existingDest = otherTmpDir() + "fileFromHome";
    createTestFile(existingDest);

    ScopedCleaner cleaner([] {
        QFile::remove(otherTmpDir() + "anotherFile");
    });

    const QList<QUrl> urls{QUrl::fromLocalFile(file1), QUrl::fromLocalFile(file2)};
    KIO::CopyJob *job = KIO::copy(urls, QUrl::fromLocalFile(otherTmpDir()), KIO::HideProgressInfo);
    if (autoSkip) {
        job->setUiDelegate(nullptr);
        job->setAutoSkip(true);
    } else {
        // Simulate the user pressing "Skip" in the dialog.
        job->setUiDelegate(new KJobUiDelegate);
        auto *askUserHandler = new MockAskUserInterface(job->uiDelegate());
        askUserHandler->m_skipResult = KIO::Result_Skip;
    }
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(otherTmpDir() + "anotherFile"));

    QCOMPARE(job->totalAmount(KJob::Files), 2); // file1, file2
    QCOMPARE(job->totalAmount(KJob::Directories), 0);
    QCOMPARE(job->processedAmount(KJob::Files), 1);
    QCOMPARE(job->processedAmount(KJob::Directories), 0);
    QCOMPARE(job->percent(), 100);
}

void JobTest::moveDestAlreadyExistsAutoRename_data()
{
    QTest::addColumn<bool>("samePartition");
    QTest::addColumn<bool>("moveDirs");

    QTest::newRow("files_same_partition") << true << false;
    QTest::newRow("files_other_partition") << false << false;
    QTest::newRow("dirs_same_partition") << true << true;
    QTest::newRow("dirs_other_partition") << false << true;
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
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QVERIFY(!QFile::exists(dir));
    }
}

void JobTest::moveDestAlreadyExistsAutoRename(const QString &destDir, bool moveDirs) // #256650
{
    const QString prefix = moveDirs ? QStringLiteral("dir ") : QStringLiteral("file ");

    const QString file1 = homeTmpDir() + prefix + "(1)";
    const QString file2 = homeTmpDir() + prefix + "(2)";
    const QString existingDest1 = destDir + prefix + "(1)";
    const QString existingDest2 = destDir + prefix + "(2)";
    const QStringList sources = QStringList{file1, file2, existingDest1, existingDest2};
    for (const QString &source : sources) {
        if (moveDirs) {
            QVERIFY(QDir().mkdir(source));
            createTestFile(source + "/innerfile");
            createTestFile(source + "/innerfile2");
        } else {
            createTestFile(source);
        }
    }
    const QString file3 = destDir + prefix + "(3)";
    const QString file4 = destDir + prefix + "(4)";

    ScopedCleaner cleaner([&]() {
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
    });

    const QList<QUrl> urls = {QUrl::fromLocalFile(file1), QUrl::fromLocalFile(file2)};
    KIO::CopyJob *job = KIO::move(urls, QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    job->setAutoRename(true);

    QSignalSpy spyRenamed(job, &KIO::CopyJob::renamed);

    //qDebug() << QDir(destDir).entryList();

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    //qDebug() << QDir(destDir).entryList();
    QVERIFY(!QFile::exists(file1)); // it was moved
    QVERIFY(!QFile::exists(file2)); // it was moved

    QVERIFY(QFile::exists(existingDest1));
    QVERIFY(QFile::exists(existingDest2));
    QVERIFY(QFile::exists(file3));
    QVERIFY(QFile::exists(file4));

    QVERIFY(!spyRenamed.isEmpty());

    auto list = spyRenamed.takeFirst();
    QCOMPARE(list.at(1).toUrl(), QUrl::fromLocalFile(destDir + prefix + "(1)"));
    QCOMPARE(list.at(2).toUrl(), QUrl::fromLocalFile(file3));

    bool samePartition = false;
    // Normally we'd see renamed(1, 3) and renamed(2, 4)
    // But across partitions, direct rename fails, and we end up with a task list of
    // 1->3, 2->3 since renaming 1 to 3 didn't happen yet.
    // so renamed(2, 3) is emitted, as if the user had chosen that.
    // And when that fails, we then get (3, 4)
    if (spyRenamed.count() == 1) {
        // It was indeed on the same partition
        samePartition = true;
        list = spyRenamed.takeFirst();
        QCOMPARE(list.at(1).toUrl(), QUrl::fromLocalFile(destDir + prefix + "(2)"));
        QCOMPARE(list.at(2).toUrl(), QUrl::fromLocalFile(file4));
    } else {
        // Remove all renamed signals about innerfiles
        spyRenamed.erase(std::remove_if(spyRenamed.begin(), spyRenamed.end(), [](const QList<QVariant> &spy){
            return spy.at(1).toUrl().path().contains("innerfile");
        }), spyRenamed.end());

        list = spyRenamed.takeFirst();
        QCOMPARE(list.at(1).toUrl(), QUrl::fromLocalFile(destDir + prefix + "(2)"));
        QCOMPARE(list.at(2).toUrl(), QUrl::fromLocalFile(file3));

        list = spyRenamed.takeFirst();
        QCOMPARE(list.at(1).toUrl(), QUrl::fromLocalFile(file3));
        QCOMPARE(list.at(2).toUrl(), QUrl::fromLocalFile(file4));
    }

    if (samePartition) {
        QCOMPARE(job->totalAmount(KJob::Files), 2); // direct-renamed, so counted as files
        QCOMPARE(job->totalAmount(KJob::Directories), 0);
        QCOMPARE(job->processedAmount(KJob::Files), 2);
        QCOMPARE(job->processedAmount(KJob::Directories), 0);
    } else {
        if (moveDirs) {
            QCOMPARE(job->totalAmount(KJob::Directories), 2);
            QCOMPARE(job->totalAmount(KJob::Files), 4); // innerfiles
            QCOMPARE(job->processedAmount(KJob::Directories), 2);
            QCOMPARE(job->processedAmount(KJob::Files), 4);
        } else {
            QCOMPARE(job->totalAmount(KJob::Files), 2);
            QCOMPARE(job->totalAmount(KJob::Directories), 0);
            QCOMPARE(job->processedAmount(KJob::Files), 2);
            QCOMPARE(job->processedAmount(KJob::Directories), 0);
        }
    }

    QCOMPARE(job->percent(), 100);
}

void JobTest::copyDirectoryAlreadyExistsSkip()
{
    // when copying a directory (which contains at least one file) to some location, and then
    // copying the same dir to the same location again, and clicking "Skip" there should be no
    // segmentation fault, bug 408350

    const QString src = homeTmpDir() + "a";
    createTestDirectory(src);
    const QString dest = homeTmpDir() + "dest";
    createTestDirectory(dest);

    QUrl u = QUrl::fromLocalFile(src);
    QUrl d = QUrl::fromLocalFile(dest);

    KIO::Job *job = KIO::copy(u, d, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest + QStringLiteral("/a/testfile")));

    job = KIO::copy(u, d, KIO::HideProgressInfo);

    // Simulate the user pressing "Skip" in the dialog.
    job->setUiDelegate(new KJobUiDelegate);
    auto *askUserHandler = new MockAskUserInterface(job->uiDelegate());
    askUserHandler->m_skipResult = KIO::Result_Skip;

    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(dest + QStringLiteral("/a/testfile")));

    QDir(src).removeRecursively();
    QDir(dest).removeRecursively();

    QCOMPARE(job->totalAmount(KJob::Files), 2); // testfile, testlink
    QCOMPARE(job->totalAmount(KJob::Directories), 1);
    QCOMPARE(job->processedAmount(KJob::Files), 0);
    QCOMPARE(job->processedAmount(KJob::Directories), 1);
    QCOMPARE(job->percent(), 0);
}

void JobTest::copyFileAlreadyExistsRename()
{
    const QString sourceFile = homeTmpDir() + "file";
    const QString dest = homeTmpDir() + "dest/";
    const QString alreadyExisting = dest + "file";
    const QString renamedFile = dest + "file-renamed";

    createTestFile(sourceFile);
    createTestFile(alreadyExisting);
    QVERIFY(QFile::exists(sourceFile));
    QVERIFY(QFile::exists(alreadyExisting));

    createTestDirectory(dest);

    ScopedCleaner cleaner([&] {
        QVERIFY(QFile(sourceFile).remove());
        QVERIFY(QDir(dest).removeRecursively());
    });

    QUrl s = QUrl::fromLocalFile(sourceFile);
    QUrl d = QUrl::fromLocalFile(dest);

    KIO::CopyJob* job = KIO::copy(s, d, KIO::HideProgressInfo);
    // Simulate the user pressing "Rename" in the dialog and choosing another destination.
    job->setUiDelegate(new KJobUiDelegate);
    auto *askUserHandler = new MockAskUserInterface(job->uiDelegate());
    askUserHandler->m_renameResult = KIO::Result_Rename;
    askUserHandler->m_newDestUrl = QUrl::fromLocalFile(renamedFile);

    QSignalSpy spyRenamed(job, &KIO::CopyJob::renamed);

    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(renamedFile));

    QCOMPARE(spyRenamed.count(), 1);
    auto list = spyRenamed.takeFirst();
    QCOMPARE(list.at(1).toUrl(), QUrl::fromLocalFile(alreadyExisting));
    QCOMPARE(list.at(2).toUrl(), QUrl::fromLocalFile(renamedFile));
}

void JobTest::safeOverwrite_data()
{
    QTest::addColumn<bool>("destFileExists");

    QTest::newRow("dest_file_exists") << true;
    QTest::newRow("dest_file_does_not_exist") << false;
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

    ScopedCleaner cleaner([&] {
        QDir(srcDir).removeRecursively();
        QDir(destDir).removeRecursively();
    });

    const int srcSize = 1000000; // ~1MB
    QVERIFY(QFile::resize(srcFile, srcSize));
    if (!destFileExists) {
        QVERIFY(QFile::remove(destFile));
    } else {
        QVERIFY(QFile::exists(destFile));
    }
    QVERIFY(!QFile::exists(destPartFile));

    if (otherTmpDirIsOnSamePartition()) {
        QSKIP(qPrintable(QStringLiteral("This test requires %1 and %2 to be on different partitions").arg(srcDir, destDir)));
    }

    KIO::FileCopyJob *job = KIO::file_move(QUrl::fromLocalFile(srcFile), QUrl::fromLocalFile(destFile), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    QSignalSpy spyTotalSize(job, &KIO::FileCopyJob::totalSize);
    connect(job, &KIO::FileCopyJob::processedSize, this, [&](KJob *job, qulonglong size) {
        Q_UNUSED(job);
        if (size > 0 && size < srcSize) {
            // To avoid overwriting dest, we want the kioslave to use dest.part
            QCOMPARE(QFileInfo::exists(destPartFile), destFileExists);
        }
    });
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFile::exists(destFile));
    QVERIFY(!QFile::exists(srcFile));
    QVERIFY(!QFile::exists(destPartFile));
    QCOMPARE(spyTotalSize.count(), 1);
}

void JobTest::overwriteOlderFiles_data()
{
    QTest::addColumn<bool>("destFileOlder");
    QTest::addColumn<bool>("moving");

    QTest::newRow("dest_file_older_copying") << true << false;
    QTest::newRow("dest_file_older_moving") << true << true;
    QTest::newRow("dest_file_younger_copying") << false << false;
    QTest::newRow("dest_file_younger_moving") << false << true;
}

void JobTest::overwriteOlderFiles()
{

    QFETCH(bool, destFileOlder);
    QFETCH(bool, moving);
    const QString srcDir = homeTmpDir() + "overwrite";
    const QString srcFile = srcDir + "/testfile";
    const QString srcFile2 = srcDir + "/testfile2";
    const QString srcFile3 = srcDir + "/testfile3";
    const QString destDir = otherTmpDir() + "overwrite_other";
    const QString destFile = destDir + "/testfile";
    const QString destFile2 = destDir + "/testfile2";
    const QString destFile3 = destDir + "/testfile3";
    const QString destPartFile = destFile + ".part";

    createTestDirectory(srcDir);
    createTestDirectory(destDir);
    createTestFile(srcFile2);
    createTestFile(srcFile3);
    createTestFile(destFile2);
    createTestFile(destFile3);
    QVERIFY(!QFile::exists(destPartFile));

    const int srcSize = 1000; // ~1KB
    QVERIFY(QFile::resize(srcFile, srcSize));
    QVERIFY(QFile::resize(srcFile2, srcSize));
    QVERIFY(QFile::resize(srcFile3, srcSize));
    if (destFileOlder) {
        setTimeStamp(destFile, QFile(srcFile).fileTime(QFileDevice::FileModificationTime).addSecs(-2));
        setTimeStamp(destFile2,  QFile(srcFile2).fileTime(QFileDevice::FileModificationTime).addSecs(-2));

        QVERIFY(QFile(destFile).fileTime(QFileDevice::FileModificationTime) <= QFile(srcFile).fileTime(QFileDevice::FileModificationTime));
        QVERIFY(QFile(destFile2).fileTime(QFileDevice::FileModificationTime) <= QFile(srcFile2).fileTime(QFileDevice::FileModificationTime));
    } else {
        setTimeStamp(destFile, QFile(srcFile).fileTime(QFileDevice::FileModificationTime).addSecs(2));
        setTimeStamp(destFile2, QFile(srcFile2).fileTime(QFileDevice::FileModificationTime).addSecs(2));

        QVERIFY(QFile(destFile).fileTime(QFileDevice::FileModificationTime) >= QFile(srcFile).fileTime(QFileDevice::FileModificationTime));
        QVERIFY(QFile(destFile2).fileTime(QFileDevice::FileModificationTime) >= QFile(srcFile2).fileTime(QFileDevice::FileModificationTime));
    }
    // to have an always skipped file
    setTimeStamp(destFile3,  QFile(srcFile3).fileTime(QFileDevice::FileModificationTime).addSecs(2));

    KIO::CopyJob *job;
    if (moving) {
        job = KIO::move(
                {QUrl::fromLocalFile(srcFile), QUrl::fromLocalFile(srcFile2), QUrl::fromLocalFile(srcFile3)},
                QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    } else {
        job = KIO::copy(
                {QUrl::fromLocalFile(srcFile), QUrl::fromLocalFile(srcFile2), QUrl::fromLocalFile(srcFile3)},
                QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    }

    job->setUiDelegate(new KJobUiDelegate);
    auto *askUserHandler = new MockAskUserInterface(job->uiDelegate());
    askUserHandler->m_renameResult = KIO::Result_OverwriteWhenOlder;

    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(askUserHandler->m_askUserRenameCalled, 1);
    QVERIFY(!QFile::exists(destPartFile));
    //QCOMPARE(spyTotalSize.count(), 1);

    // skipped file whose dest is always newer
    QVERIFY(QFile::exists(srcFile3)); // it was skipped
    QCOMPARE(QFile(destFile3).size(), 11);

    if (destFileOlder) {
        // files were overwritten
        QCOMPARE(QFile(destFile).size(), 1000);
        QCOMPARE(QFile(destFile2).size(), 1000);

        // files were overwritten
        QCOMPARE(job->processedAmount(KJob::Files), 2);
        QCOMPARE(job->processedAmount(KJob::Directories), 0);

        if (moving) {
            QVERIFY(!QFile::exists(srcFile)); // it was moved
            QVERIFY(!QFile::exists(srcFile2)); // it was moved
        } else {
            QVERIFY(QFile::exists(srcFile)); // it was copied
            QVERIFY(QFile::exists(srcFile2)); // it was copied

            QCOMPARE(QFile(destFile).fileTime(QFileDevice::FileModificationTime),
                     QFile(srcFile).fileTime(QFileDevice::FileModificationTime));
            QCOMPARE(QFile(destFile2).fileTime(QFileDevice::FileModificationTime),
                     QFile(srcFile2).fileTime(QFileDevice::FileModificationTime));
        }
    } else {
        // files were skipped
        QCOMPARE(job->processedAmount(KJob::Files), 0);
        QCOMPARE(job->processedAmount(KJob::Directories), 0);

        QCOMPARE(QFile(destFile).size(), 11);
        QCOMPARE(QFile(destFile2).size(), 11);

        QVERIFY(QFile::exists(srcFile));
        QVERIFY(QFile::exists(srcFile2));
    }

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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(!QFile::exists(sourceFile)); // it was moved

#ifndef Q_OS_WIN
    // Now same thing when the target is a symlink to the source
    createTestFile(sourceFile);
    createTestSymlink(existingDest, QFile::encodeName(sourceFile));
    QVERIFY(QFile::exists(existingDest));
    job = KIO::file_move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(!QFile::exists(sourceFile)); // it was moved

    // Now same thing when the target is a symlink to another file
    createTestFile(sourceFile);
    createTestFile(sourceFile + QLatin1Char('2'));
    createTestSymlink(existingDest, QFile::encodeName(sourceFile + QLatin1Char('2')));
    QVERIFY(QFile::exists(existingDest));
    job = KIO::file_move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(!QFile::exists(sourceFile)); // it was moved

    // Now same thing when the target is a _broken_ symlink
    createTestFile(sourceFile);
    createTestSymlink(existingDest);
    QVERIFY(!QFile::exists(existingDest)); // it exists, but it's broken...
    job = KIO::file_move(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(existingDest), -1, KIO::HideProgressInfo | KIO::Overwrite);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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
    QVERIFY(!job->exec());
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

    ScopedCleaner cleaner([&] {
        QDir(destDir).removeRecursively();
    });

    // With KIO::link (high-level)
    KIO::CopyJob *job = KIO::link(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(destDir), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFileInfo::exists(sourceFile));
    const QString dest = destDir + "/fileFromHome";
    QVERIFY(QFileInfo(dest).isSymLink());
    QCOMPARE(QFileInfo(dest).symLinkTarget(), sourceFile);
    QFile::remove(dest);

    // With KIO::symlink (low-level)
    const QString linkPath = destDir + "/link";
    KIO::Job *symlinkJob = KIO::symlink(sourceFile, QUrl::fromLocalFile(linkPath), KIO::HideProgressInfo);
    QVERIFY2(symlinkJob->exec(), qPrintable(symlinkJob->errorString()));
    QVERIFY(QFileInfo::exists(sourceFile));
    QVERIFY(QFileInfo(linkPath).isSymLink());
    QCOMPARE(QFileInfo(linkPath).symLinkTarget(), sourceFile);
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

    ScopedCleaner cleaner([&] {
        QVERIFY(QFile::remove(dest));
    });

    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFileInfo::exists(sourceFile));
    QVERIFY(QFileInfo(dest).isSymLink());
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

    ScopedCleaner cleaner([&] {
        QVERIFY(QDir().rmdir(dest));
    });

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

    ScopedCleaner cleaner([&] {
        QVERIFY(QFile::remove(sourceFile));
        QVERIFY(QFile::remove(dest));
    });

    // First time works
    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFileInfo(dest).isSymLink());

    // Second time fails (already exists)
    job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), (int)KIO::ERR_FILE_ALREADY_EXIST);

    // KIO::symlink fails too
    KIO::Job *symlinkJob = KIO::symlink(sourceFile, QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY(!symlinkJob->exec());
    QCOMPARE(symlinkJob->error(), (int)KIO::ERR_FILE_ALREADY_EXIST);
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

    ScopedCleaner cleaner([&] {
        QVERIFY(QFile::remove(sourceFile));
        QVERIFY(QFile::remove(dest));
    });

    // First time works
    KIO::CopyJob *job = KIO::linkAs(QUrl::fromLocalFile(sourceFile), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFileInfo(dest).isSymLink());

    // Changing the link target, with overwrite, works
    job = KIO::linkAs(QUrl::fromLocalFile(sourceFile + QLatin1Char('2')), QUrl::fromLocalFile(dest), KIO::Overwrite | KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QVERIFY(QFileInfo(dest).isSymLink());
    QCOMPARE(QFileInfo(dest).symLinkTarget(), QString(sourceFile + QLatin1Char('2')));

    // Changing the link target using KIO::symlink, with overwrite, works
    KIO::Job *symlinkJob = KIO::symlink(sourceFile + QLatin1Char('3'), QUrl::fromLocalFile(dest), KIO::Overwrite | KIO::HideProgressInfo);
    QVERIFY2(symlinkJob->exec(), qPrintable(symlinkJob->errorString()));
    QVERIFY(QFileInfo(dest).isSymLink());
    QCOMPARE(QFileInfo(dest).symLinkTarget(), QString(sourceFile + QLatin1Char('3')));
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
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
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

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    QSignalSpy spyData(job, &KIO::MultiGetJob::data);
#else
    QSignalSpy spyData(job, &KIO::MultiGetJob::dataReceived);
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QSignalSpy spyMimeType(job, SIGNAL(mimetype(long,QString)));
#endif
    QSignalSpy spyMimeTypeFound(job, &KIO::MultiGetJob::mimeTypeFound);

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    QSignalSpy spyResultId(job, QOverload<long>::of(&KIO::MultiGetJob::result));
#else
    QSignalSpy spyResultId(job, &KIO::MultiGetJob::fileTransferred);
#endif

    QSignalSpy spyResult(job, &KJob::result);
    job->setUiDelegate(nullptr);

    for (int i = 1; i < numFiles; ++i) {
        const QUrl url = urls.at(i);
        job->get(i, url, KIO::MetaData());
    }
    //connect(job, &KIO::MultiGetJob::result, [=] (long id) { qDebug() << "ID I got" << id;});
    //connect(job, &KJob::result, [this](KJob* ) {qDebug() << "END";});

    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QCOMPARE(spyResult.count(), 1);
    QCOMPARE(spyResultId.count(), numFiles);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    QCOMPARE(spyMimeType.count(), numFiles);
#endif
    QCOMPARE(spyMimeTypeFound.count(), numFiles);
    QCOMPARE(spyData.count(), numFiles * 2);
    for (int i = 0; i < numFiles; ++i) {
        QCOMPARE(spyResultId.at(i).at(0).toInt(), i);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
        QCOMPARE(spyMimeType.at(i).at(0).toInt(), i);
        QCOMPARE(spyMimeType.at(i).at(1).toString(), QStringLiteral("text/plain"));
#endif
        QCOMPARE(spyMimeTypeFound.at(i).at(0).toInt(), i);
        QCOMPARE(spyMimeTypeFound.at(i).at(1).toString(), QStringLiteral("text/plain"));
        QCOMPARE(spyData.at(i * 2).at(0).toInt(), i);
        QCOMPARE(QString(spyData.at(i * 2).at(1).toByteArray()), QStringLiteral("Hello"));
        QCOMPARE(spyData.at(i * 2 + 1).at(0).toInt(), i);
        QCOMPARE(QString(spyData.at(i * 2 + 1).at(1).toByteArray()), QLatin1String(""));
    }
}

void JobTest::cancelCopyAndCleanDest_data()
{
    QTest::addColumn<bool>("suspend");
    QTest::addColumn<bool>("overwrite");

    QTest::newRow("suspend_no_overwrite") << true << false;
    QTest::newRow("no_suspend_no_overwrite") << false << false;

#ifndef Q_OS_WIN
    QTest::newRow("suspend_with_overwrite") << true << true;
    QTest::newRow("no_suspend_with_overwrite") << false << true;
#endif
}

void JobTest::cancelCopyAndCleanDest()
{
    QFETCH(bool, suspend);
    QFETCH(bool, overwrite);

    const QString baseDir = homeTmpDir();
    const QString srcTemplate = baseDir + QStringLiteral("testfile_XXXXXX");
    const QString destFile = baseDir + QStringLiteral("testfile_copy_slow_") + QString::fromLatin1(QTest::currentDataTag());

    QTemporaryFile f(srcTemplate);
    if (!f.open()) {
        qFatal("Couldn't open %s", qPrintable(f.fileName()));
    }
    f.seek(999999);
    f.write("0");
    f.close();
    QCOMPARE(f.size(), 1000000); //~1MB

    if (overwrite) {
        createTestFile(destFile);
    }
    const QString destToCheck = (overwrite) ? destFile + QStringLiteral(".part") : destFile;

    KIO::JobFlag m_overwriteFlag = overwrite ? KIO::Overwrite : KIO::DefaultFlags;
    KIO::FileCopyJob *copyJob = KIO::file_copy(QUrl::fromLocalFile(f.fileName()), QUrl::fromLocalFile(destFile), -1, KIO::HideProgressInfo | m_overwriteFlag);
    copyJob->setUiDelegate(nullptr);
    QSignalSpy spyProcessedSize(copyJob, &KIO::Job::processedSize);
    QSignalSpy spyFinished(copyJob, &KIO::Job::finished);
    connect(copyJob, &KIO::Job::processedSize, this, [destFile, suspend, destToCheck](KJob *job, qulonglong processedSize) {
        if (processedSize > 0) {
            QVERIFY2(QFile::exists(destToCheck), qPrintable(destToCheck));
            if (suspend) {
                job->suspend();
            }
            QVERIFY(job->kill());
        }
    });

    QVERIFY(!copyJob->exec());
    QCOMPARE(spyProcessedSize.count(), 1);
    QCOMPARE(spyFinished.count(), 1);
    QCOMPARE(copyJob->error(), KIO::ERR_USER_CANCELED);

    // the destination file actual deletion happens after finished() is emitted
    // we need to give some time to the ioslave to finish the file cleaning
    QTRY_VERIFY2(!QFile::exists(destToCheck), qPrintable(destToCheck));
}

