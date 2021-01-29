/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2006 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2008 Norbert Frese <nf2@scheinwelt.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "jobremotetest.h"

#include <QTest>

#include <QDebug>
#include <KLocalizedString>

#include <QEventLoop>
#include <QDir>
#include <QUrl>

#include <kprotocolinfo.h>
#include <kio/scheduler.h>
#include <kio/directorysizejob.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/filejob.h>
#include <QStandardPaths>
//#include "kiotesthelper.h" // createTestFile etc.

QTEST_MAIN(JobRemoteTest)

QDateTime s_referenceTimeStamp;

// The code comes partly from jobtest.cpp

static QUrl remoteTmpUrl()
{
    QString customDir(qgetenv("KIO_JOBREMOTETEST_REMOTETMP"));
    if (customDir.isEmpty()) {
        return QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + '/');
    } else {
        // Could be a path or a URL
        return QUrl::fromUserInput(customDir + '/');
    }
}

static QString localTmpDir()
{
#ifdef Q_OS_WIN
    return QDir::tempPath() + "/jobremotetest/";
#else
    // This one needs to be on another partition
    return QStringLiteral("/tmp/jobremotetest/");
#endif
}

static bool myExists(const QUrl &url)
{
    KIO::Job *job = KIO::statDetails(url, KIO::StatJob::DestinationSide, KIO::StatBasic, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    return job->exec();
}

static bool myMkdir(const QUrl &url)
{
    KIO::Job *job = KIO::mkdir(url, -1);
    job->setUiDelegate(nullptr);
    return job->exec();
}

void JobRemoteTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");

    s_referenceTimeStamp = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago

    // Start with a clean base dir
    cleanupTestCase();
    QUrl url = remoteTmpUrl();
    if (!myExists(url)) {
        const bool ok = url.isLocalFile() ? QDir().mkpath(url.toLocalFile()) : myMkdir(url);
        if (!ok) {
            qFatal("couldn't create %s", qPrintable(url.toString()));
        }
    }
    const bool ok = QDir().mkpath(localTmpDir());
    if (!ok) {
        qFatal("couldn't create %s", qPrintable(localTmpDir()));
    }
}

static void delDir(const QUrl &pathOrUrl)
{
    KIO::Job *job = KIO::del(pathOrUrl, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->exec();
}

void JobRemoteTest::cleanupTestCase()
{
    delDir(remoteTmpUrl());
    delDir(QUrl::fromLocalFile(localTmpDir()));
}

void JobRemoteTest::enterLoop()
{
    QEventLoop eventLoop;
    connect(this, &JobRemoteTest::exitLoop,
            &eventLoop, &QEventLoop::quit);
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

/////

void JobRemoteTest::putAndGet()
{
    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "putAndGetFile");
    KIO::TransferJob *job = KIO::put(u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    job->setModificationTime(mtime);
    job->setUiDelegate(nullptr);
    connect(job, &KJob::result,
            this, &JobRemoteTest::slotResult);
    connect(job, &KIO::TransferJob::dataReq,
            this, &JobRemoteTest::slotDataReq);
    m_result = -1;
    m_dataReqCount = 0;
    enterLoop();
    QVERIFY(m_result == 0);   // no error

    m_result = -1;

    KIO::StoredTransferJob *getJob = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
    getJob->setUiDelegate(nullptr);
    connect(getJob, &KJob::result,
            this, &JobRemoteTest::slotGetResult);
    enterLoop();
    QCOMPARE(m_result, 0);   // no error
    QCOMPARE(m_data, QByteArray("This is a test for KIO::put()\n"));
    //QCOMPARE( m_data.size(), 11 );
}

void JobRemoteTest::slotGetResult(KJob *job)
{
    m_result = job->error();
    m_data = static_cast<KIO::StoredTransferJob *>(job)->data();
    Q_EMIT exitLoop();
}

void JobRemoteTest::slotDataReq(KIO::Job *, QByteArray &data)
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

void JobRemoteTest::slotResult(KJob *job)
{
    m_result = job->error();
    Q_EMIT exitLoop();
}

////

void JobRemoteTest::openFileWriting()
{
    m_rwCount = 0;

    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "openFileWriting");
    fileJob = KIO::open(u, QIODevice::WriteOnly);

    fileJob->setUiDelegate(nullptr);
    connect(fileJob, &KJob::result,
            this, &JobRemoteTest::slotResult);
    connect(fileJob, &KIO::FileJob::data,
            this, &JobRemoteTest::slotFileJobData);
    connect(fileJob, &KIO::FileJob::open,
            this, &JobRemoteTest::slotFileJobOpen);
    connect(fileJob, &KIO::FileJob::written,
            this, &JobRemoteTest::slotFileJobWritten);
    connect(fileJob, &KIO::FileJob::position,
            this, &JobRemoteTest::slotFileJobPosition);

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    connect(fileJob, QOverload<KIO::Job*>::of(&KIO::FileJob::close),
            this, &JobRemoteTest::slotFileJobClose);
#else
    connect(fileJob, &KIO::FileJob::fileClosed, this, &JobRemoteTest::slotFileJobClose);
#endif

    m_result = -1;
    m_closeSignalCalled = false;

    enterLoop();
    QEXPECT_FAIL("", "Needs fixing in kio_file", Abort);
    QVERIFY(m_result == 0);   // no error

    KIO::StoredTransferJob *getJob = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
    getJob->setUiDelegate(nullptr);
    connect(getJob, &KJob::result,
            this, &JobRemoteTest::slotGetResult);
    enterLoop();
    QCOMPARE(m_result, 0);   // no error
    QVERIFY(m_closeSignalCalled); // close signal called.
    qDebug() << "m_data: " << m_data;
    QCOMPARE(m_data, QByteArray("test....test....test....test....test....test....end"));

}

void JobRemoteTest::slotFileJobData(KIO::Job *job, const QByteArray &data)
{
    Q_UNUSED(job);
    Q_UNUSED(data);
}

void JobRemoteTest::slotFileJobRedirection(KIO::Job *job, const QUrl &url)
{
    Q_UNUSED(job);
    Q_UNUSED(url);
}

void JobRemoteTest::slotFileJobMimetype(KIO::Job *job, const QString &type)
{
    Q_UNUSED(job);
    Q_UNUSED(type);
}

void JobRemoteTest::slotFileJobOpen(KIO::Job *job)
{
    Q_UNUSED(job);
    fileJob->seek(0);
}

void JobRemoteTest::slotFileJobWritten(KIO::Job *job, KIO::filesize_t written)
{
    Q_UNUSED(job);
    Q_UNUSED(written);
    if (m_rwCount > 5) {
        fileJob->close();
    } else {
        fileJob->seek(m_rwCount * 8);
        m_rwCount++;
    }
}

void JobRemoteTest::slotFileJobPosition(KIO::Job *job, KIO::filesize_t offset)
{
    Q_UNUSED(job);
    Q_UNUSED(offset);
    const QByteArray data("test....end");
    fileJob->write(data);

}

void JobRemoteTest::slotFileJobClose(KIO::Job *job)
{
    Q_UNUSED(job);
    m_closeSignalCalled = true;
    qDebug() << "+++++++++ filejob closed";
}

////

void JobRemoteTest::openFileReading()
{
    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "openFileReading");

    const QByteArray putData("test1test2test3test4test5");

    KIO::StoredTransferJob *putJob = KIO::storedPut(putData,
                                     u,
                                     0600, KIO::Overwrite | KIO::HideProgressInfo
                                                   );

    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    putJob->setModificationTime(mtime);
    putJob->setUiDelegate(nullptr);
    connect(putJob, &KJob::result,
            this, &JobRemoteTest::slotResult);
    m_result = -1;
    enterLoop();
    QVERIFY(m_result == 0);   // no error

    m_rwCount = 4;
    m_data = QByteArray();

    fileJob = KIO::open(u, QIODevice::ReadOnly);

    fileJob->setUiDelegate(nullptr);
    connect(fileJob, &KJob::result,
            this, &JobRemoteTest::slotResult);
    connect(fileJob, &KIO::FileJob::data,
            this, &JobRemoteTest::slotFileJob2Data);
    connect(fileJob, &KIO::FileJob::open,
            this, &JobRemoteTest::slotFileJob2Open);
    connect(fileJob, &KIO::FileJob::written,
            this, &JobRemoteTest::slotFileJob2Written);
    connect(fileJob, &KIO::FileJob::position,
            this, &JobRemoteTest::slotFileJob2Position);

    // Can reuse this slot (same for all tests).
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    connect(fileJob, QOverload<KIO::Job*>::of(&KIO::FileJob::close), this, &JobRemoteTest::slotFileJobClose);
#else
    connect(fileJob, &KIO::FileJob::fileClosed, this, &JobRemoteTest::slotFileJobClose);
#endif

    m_result = -1;
    m_closeSignalCalled = false;

    enterLoop();
    QVERIFY(m_result == 0);   // no error
    QVERIFY(m_closeSignalCalled); // close signal called.
    qDebug() << "resulting m_data: " << QString(m_data);
    QCOMPARE(m_data, QByteArray("test5test4test3test2test1"));

}

void JobRemoteTest::slotFileJob2Data(KIO::Job *job, const QByteArray &data)
{
    Q_UNUSED(job);
    qDebug() << "m_rwCount = " << m_rwCount << " data: " << data;
    m_data.append(data);

    if (m_rwCount < 0) {
        fileJob->close();
    } else {
        fileJob->seek(m_rwCount-- * 5);
    }
}

void JobRemoteTest::slotFileJob2Redirection(KIO::Job *job, const QUrl &url)
{
    Q_UNUSED(job);
    Q_UNUSED(url);
}

void JobRemoteTest::slotFileJob2Mimetype(KIO::Job *job, const QString &type)
{
    Q_UNUSED(job);
    qDebug() << "MIME type: " << type;
}

void JobRemoteTest::slotFileJob2Open(KIO::Job *job)
{
    Q_UNUSED(job);
    fileJob->seek(m_rwCount-- * 5);
}

void JobRemoteTest::slotFileJob2Written(KIO::Job *job, KIO::filesize_t written)
{
    Q_UNUSED(job);
    Q_UNUSED(written);
}

void JobRemoteTest::slotFileJob2Position(KIO::Job *job, KIO::filesize_t offset)
{
    Q_UNUSED(job);
    qDebug() << "position : " << offset << " -> read (5)";
    fileJob->read(5);
}

////

void JobRemoteTest::slotMimetype(KIO::Job *job, const QString &type)
{
    QVERIFY(job != nullptr);
    m_mimetype = type;
}

void JobRemoteTest::openFileRead0Bytes()
{
    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "openFileReading");

    const QByteArray putData("Doesn't matter");

    KIO::StoredTransferJob *putJob = KIO::storedPut(putData,
                                     u,
                                     0600, KIO::Overwrite | KIO::HideProgressInfo
                                                   );

    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    putJob->setModificationTime(mtime);
    putJob->setUiDelegate(nullptr);
    connect(putJob, &KJob::result,
            this, &JobRemoteTest::slotResult);
    m_result = -1;
    enterLoop();
    QVERIFY(m_result == 0);   // no error

    m_data = QByteArray();

    fileJob = KIO::open(u, QIODevice::ReadOnly);

    fileJob->setUiDelegate(nullptr);
    connect(fileJob, &KJob::result,
            this, &JobRemoteTest::slotResult);
    connect(fileJob, &KIO::FileJob::data,
            this, &JobRemoteTest::slotFileJob3Data);
    connect(fileJob, &KIO::FileJob::open,
            this, &JobRemoteTest::slotFileJob3Open);
    // Can reuse this slot (it's a noop).
    connect(fileJob, &KIO::FileJob::written,
            this, &JobRemoteTest::slotFileJob2Written);
    connect(fileJob, &KIO::FileJob::position,
            this, &JobRemoteTest::slotFileJob3Position);

    // Can reuse this as well.
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    connect(fileJob, QOverload<KIO::Job*>::of(&KIO::FileJob::close), this, &JobRemoteTest::slotFileJobClose);
#else
    connect(fileJob, &KIO::FileJob::fileClosed, this, &JobRemoteTest::slotFileJobClose);
#endif

    m_result = -1;
    m_closeSignalCalled = false;

    enterLoop();
    // Previously reading 0 bytes would cause both data() and error() being emitted...
    QVERIFY(m_result == 0);   // no error
    QVERIFY(m_closeSignalCalled); // close signal called.
}

void JobRemoteTest::slotFileJob3Open(KIO::Job *job)
{
    Q_UNUSED(job);
    fileJob->seek(0);
}

void JobRemoteTest::slotFileJob3Position(KIO::Job *job, KIO::filesize_t offset)
{
    Q_UNUSED(job);
    qDebug() << "position : " << offset << " -> read (0)";
    fileJob->read(0);
}

void JobRemoteTest::slotFileJob3Data(KIO::Job *job, const QByteArray& data)
{
    Q_UNUSED(job);
    QVERIFY(data.isEmpty());
    fileJob->close();
}

void JobRemoteTest::openFileTruncating()
{
    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "openFileTruncating");

    const QByteArray putData("test1");

    KIO::StoredTransferJob *putJob = KIO::storedPut(putData,
                                     u,
                                     0600, KIO::Overwrite | KIO::HideProgressInfo
                                                   );

    quint64 secsSinceEpoch = QDateTime::currentSecsSinceEpoch(); // Use second granularity, supported on all filesystems
    QDateTime mtime = QDateTime::fromSecsSinceEpoch(secsSinceEpoch - 30); // 30 seconds ago
    putJob->setModificationTime(mtime);
    putJob->setUiDelegate(nullptr);
    connect(putJob, &KJob::result,
            this, &JobRemoteTest::slotResult);
    m_result = -1;
    enterLoop();
    QVERIFY(m_result == 0);   // no error

    m_truncatedFile.setFileName(u.toLocalFile());
    QVERIFY(m_truncatedFile.exists());
    QVERIFY(m_truncatedFile.open(QIODevice::ReadOnly));
    fileJob = KIO::open(u, QIODevice::ReadWrite);

    fileJob->setUiDelegate(nullptr);
    connect(fileJob, &KJob::result,
            this, &JobRemoteTest::slotResult);
    connect(fileJob, &KIO::FileJob::open,
            this, &JobRemoteTest::slotFileJob4Open);
    connect(fileJob, &KIO::FileJob::truncated,
            this, &JobRemoteTest::slotFileJob4Truncated);

    // Can reuse this slot (same for all tests).
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    connect(fileJob, QOverload<KIO::Job*>::of(&KIO::FileJob::close), this, &JobRemoteTest::slotFileJobClose);
#else
    connect(fileJob, &KIO::FileJob::fileClosed, this, &JobRemoteTest::slotFileJobClose);
#endif

    m_result = -1;
    m_closeSignalCalled = false;

    enterLoop();
    QVERIFY(m_result == 0);   // no error
    QVERIFY(m_closeSignalCalled); // close signal called.
}

void JobRemoteTest::slotFileJob4Open(KIO::Job *job)
{
    Q_UNUSED(job);
    fileJob->truncate(10);
    qDebug() << "Truncating file to 10";
}

void JobRemoteTest::slotFileJob4Truncated(KIO::Job *job, KIO::filesize_t length)
{
    Q_UNUSED(job);
    if(length == 10) {
        m_truncatedFile.seek(0);
        QCOMPARE(m_truncatedFile.readAll(), QByteArray("test1\x00\x00\x00\x00\x00", 10));
        fileJob->truncate(4);
        qDebug() << "Truncating file to 4";
    } else if(length == 4) {
        m_truncatedFile.seek(0);
        QCOMPARE(m_truncatedFile.readAll(), QByteArray("test"));
        fileJob->truncate(0);
        qDebug() << "Truncating file to 0";
    } else {
        m_truncatedFile.seek(0);
        QCOMPARE(m_truncatedFile.readAll(), QByteArray());
        fileJob->close();
        qDebug() << "Truncating file finished";
    }
}
