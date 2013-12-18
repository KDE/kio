/* This file is part of the KDE project
   Copyright (C) 2004-2006 David Faure <faure@kde.org>
   Copyright (C) 2008      Norbert Frese <nf2@scheinwelt.at>

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

#include "jobremotetest.h"

#include <QDebug>
#include <klocalizedstring.h>

#include <QtCore/QFileInfo>
#include <QtCore/QEventLoop>
#include <QtCore/QDir>
#include <QtCore/QVariant>
#include <QUrl>

#include <kprotocolinfo.h>
#include <kio/scheduler.h>
#include <kio/directorysizejob.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/filejob.h>
#include <qstandardpaths.h>
//#include "kiotesthelper.h" // createTestFile etc.

QTEST_MAIN(JobRemoteTest)

QDateTime s_referenceTimeStamp;

// The code comes partly from jobtest.cpp

static QUrl remoteTmpUrl()
{
    QString customDir(qgetenv("KIO_JOBREMOTETEST_REMOTETMP"));
    if (customDir.isEmpty()) {
        return QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + '/');
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
    return "/tmp/jobremotetest/";
#endif
}

static bool myExists(const QUrl& url) {
    KIO::Job* job = KIO::stat(url, KIO::StatJob::DestinationSide, 0, KIO::HideProgressInfo);
    job->setUiDelegate(0);
    return job->exec();
}

static bool myMkdir(const QUrl& url) {
    KIO::Job* job = KIO::mkdir(url, -1);
    job->setUiDelegate(0);
    return job->exec();
}

void JobRemoteTest::initTestCase()
{
    QStandardPaths::enableTestMode(true);

    s_referenceTimeStamp = QDateTime::currentDateTime().addSecs( -30 ); // 30 seconds ago

    // Start with a clean base dir
    cleanupTestCase();
    QUrl url = remoteTmpUrl();
    if (!myExists(url)) {
        const bool ok = url.isLocalFile() ? QDir().mkpath(url.toLocalFile()) : myMkdir(url);
        if ( !ok )
            qFatal("couldn't create %s", qPrintable(url.toString()));
    }
    const bool ok = QDir().mkpath(localTmpDir());
    if ( !ok )
        qFatal("couldn't create %s", qPrintable(localTmpDir()));
}

static void delDir(const QUrl& pathOrUrl) {
    KIO::Job* job = KIO::del(pathOrUrl, KIO::HideProgressInfo);
    job->setUiDelegate(0);
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
    connect(this, SIGNAL(exitLoop()),
            &eventLoop, SLOT(quit()));
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

/////

void JobRemoteTest::putAndGet()
{
    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "putAndGetFile");
    KIO::TransferJob* job = KIO::put(u, 0600, KIO::Overwrite | KIO::HideProgressInfo);
    QDateTime mtime = QDateTime::currentDateTime().addSecs( -30 ); // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    job->setModificationTime(mtime);
    job->setUiDelegate( 0 );
    connect( job, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)) );
    connect( job, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
             this, SLOT(slotDataReq(KIO::Job*,QByteArray&)) );
    m_result = -1;
    m_dataReqCount = 0;
    enterLoop();
    QVERIFY( m_result == 0 ); // no error

    m_result = -1;

    KIO::StoredTransferJob* getJob = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
    getJob->setUiDelegate( 0 );
    connect( getJob, SIGNAL(result(KJob*)),
            this, SLOT(slotGetResult(KJob*)) );
    enterLoop();
    QCOMPARE( m_result, 0 ); // no error
    QCOMPARE( m_data, QByteArray("This is a test for KIO::put()\n") );
    //QCOMPARE( m_data.size(), 11 );
}

void JobRemoteTest::slotGetResult( KJob* job )
{
    m_result = job->error();
    m_data = static_cast<KIO::StoredTransferJob *>(job)->data();
    emit exitLoop();
}


void JobRemoteTest::slotDataReq( KIO::Job*, QByteArray& data )
{
    // Really not the way you'd write a slotDataReq usually :)
    switch(m_dataReqCount++) {
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

void JobRemoteTest::slotResult( KJob* job )
{
    m_result = job->error();
    emit exitLoop();
}

////

void JobRemoteTest::openFileWriting()
{
    m_rwCount = 0;

    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "openFileWriting");
    fileJob = KIO::open(u, QIODevice::WriteOnly);

    fileJob->setUiDelegate( 0 );
    connect( fileJob, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)) );
    connect( fileJob, SIGNAL(data(KIO::Job*,QByteArray)),
             this, SLOT(slotFileJobData(KIO::Job*,QByteArray)) );
    connect( fileJob, SIGNAL(open(KIO::Job*)),
             this, SLOT(slotFileJobOpen(KIO::Job*)) );
    connect( fileJob, SIGNAL(written(KIO::Job*,KIO::filesize_t)),
             this, SLOT(slotFileJobWritten(KIO::Job*,KIO::filesize_t)) );
    connect( fileJob, SIGNAL(position(KIO::Job*,KIO::filesize_t)),
             this, SLOT(slotFileJobPosition(KIO::Job*,KIO::filesize_t)) );
    connect( fileJob, SIGNAL(close(KIO::Job*)),
             this, SLOT(slotFileJobClose(KIO::Job*)) );
    m_result = -1;

    enterLoop();
    QEXPECT_FAIL("", "Needs fixing in kio_file", Abort);
    QVERIFY( m_result == 0 ); // no error

    KIO::StoredTransferJob* getJob = KIO::storedGet(u, KIO::NoReload, KIO::HideProgressInfo);
    getJob->setUiDelegate( 0 );
    connect( getJob, SIGNAL(result(KJob*)),
             this, SLOT(slotGetResult(KJob*)) );
    enterLoop();
    QCOMPARE( m_result, 0 ); // no error
    qDebug() << "m_data: " << m_data;
    QCOMPARE( m_data, QByteArray("test....test....test....test....test....test....end") );

}

void JobRemoteTest::slotFileJobData (KIO::Job *job, const QByteArray &data)
{
    Q_UNUSED(job);
    Q_UNUSED(data);
}

void JobRemoteTest::slotFileJobRedirection (KIO::Job *job, const QUrl &url)
{
    Q_UNUSED(job);
    Q_UNUSED(url);
}

void JobRemoteTest::slotFileJobMimetype (KIO::Job *job, const QString &type)
{
    Q_UNUSED(job);
    Q_UNUSED(type);
}

void JobRemoteTest::slotFileJobOpen (KIO::Job *job)
{
    Q_UNUSED(job);
    fileJob->seek (0);
}

void JobRemoteTest::slotFileJobWritten (KIO::Job *job, KIO::filesize_t written)
{
    Q_UNUSED(job);
    Q_UNUSED(written);
    if (m_rwCount > 5) {
        fileJob->close();
    } else {
        fileJob->seek (m_rwCount*8);
        m_rwCount++;
    }
}

void JobRemoteTest::slotFileJobPosition (KIO::Job *job, KIO::filesize_t offset)
{
    Q_UNUSED(job);
    Q_UNUSED(offset);
    const QByteArray data("test....end");
    fileJob->write (data);

}


void JobRemoteTest::slotFileJobClose (KIO::Job *job)
{
    Q_UNUSED(job);
    qDebug() << "+++++++++ closed";
}

////

void JobRemoteTest::openFileReading()
{
    QUrl u(remoteTmpUrl());
    u.setPath(u.path() + "openFileReading");

    const QByteArray putData("test1test2test3test4test5");

    KIO::StoredTransferJob * putJob = KIO::storedPut( putData,
            u,
            0600, KIO::Overwrite | KIO::HideProgressInfo
        );

    QDateTime mtime = QDateTime::currentDateTime().addSecs( -30 ); // 30 seconds ago
    mtime.setTime_t(mtime.toTime_t()); // hack for losing the milliseconds
    putJob->setModificationTime(mtime);
    putJob->setUiDelegate( 0 );
    connect( putJob, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)) );
    m_result = -1;
    enterLoop();
    QVERIFY( m_result == 0 ); // no error


    m_rwCount = 4;
    m_data = QByteArray();

    fileJob = KIO::open(u, QIODevice::ReadOnly);

    fileJob->setUiDelegate( 0 );
    connect( fileJob, SIGNAL(result(KJob*)),
            this, SLOT(slotResult(KJob*)) );
    connect( fileJob, SIGNAL(data(KIO::Job*,QByteArray)),
             this, SLOT(slotFileJob2Data(KIO::Job*,QByteArray)) );
    connect( fileJob, SIGNAL(open(KIO::Job*)),
             this, SLOT(slotFileJob2Open(KIO::Job*)) );
    connect( fileJob, SIGNAL(written(KIO::Job*,KIO::filesize_t)),
             this, SLOT(slotFileJob2Written(KIO::Job*,KIO::filesize_t)) );
    connect( fileJob, SIGNAL(position(KIO::Job*,KIO::filesize_t)),
             this, SLOT(slotFileJob2Position(KIO::Job*,KIO::filesize_t)) );
    connect( fileJob, SIGNAL(close(KIO::Job*)),
             this, SLOT(slotFileJob2Close(KIO::Job*)) );
    m_result = -1;

    enterLoop();
    QVERIFY( m_result == 0 ); // no error
    qDebug() << "resulting m_data: " << QString(m_data);
    QCOMPARE( m_data, QByteArray("test5test4test3test2test1"));

}

void JobRemoteTest::slotFileJob2Data (KIO::Job *job, const QByteArray &data)
{
    Q_UNUSED(job);
    qDebug() << "m_rwCount = " << m_rwCount << " data: " << data;
    m_data.append(data);

    if (m_rwCount < 0) {
        fileJob->close();
    } else {
        fileJob->seek (m_rwCount--*5);
    }
}

void JobRemoteTest::slotFileJob2Redirection (KIO::Job *job, const QUrl &url)
{
    Q_UNUSED(job);
    Q_UNUSED(url);
}

void JobRemoteTest::slotFileJob2Mimetype (KIO::Job *job, const QString &type)
{
    Q_UNUSED(job);
    qDebug() << "mimetype: " << type;
}

void JobRemoteTest::slotFileJob2Open (KIO::Job *job)
{
    Q_UNUSED(job);
    fileJob->seek (m_rwCount--*5);
}

void JobRemoteTest::slotFileJob2Written (KIO::Job *job, KIO::filesize_t written)
{
    Q_UNUSED(job);
    Q_UNUSED(written);
}

void JobRemoteTest::slotFileJob2Position (KIO::Job *job, KIO::filesize_t offset)
{
    Q_UNUSED(job);
    qDebug() << "position : " << offset << " -> read (5)";
    fileJob->read (5);
}


void JobRemoteTest::slotFileJob2Close (KIO::Job *job)
{
    Q_UNUSED(job);
    qDebug() << "+++++++++ job2 closed";
}

////

void JobRemoteTest::slotMimetype(KIO::Job* job, const QString& type)
{
    QVERIFY( job != 0 );
    m_mimetype = type;
}

