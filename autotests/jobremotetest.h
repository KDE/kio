/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2008 Norbert Frese <nf2@scheinwelt.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

/*
   Please set KIO_JOBREMOTETEST_REMOTETMP to test other protocols than kio_file.
   Don't forget the trailing slash!
*/

#ifndef JOBREMOTETEST_H
#define JOBREMOTETEST_H

#include <QString>
#include <QObject>
#include <kio/job.h>

class JobRemoteTest : public QObject
{
    Q_OBJECT

public:
    JobRemoteTest() {}

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    // Remote tests
    void putAndGet();
    void openFileWriting();
    void openFileReading();
    void openFileRead0Bytes();
    void openFileTruncating();

    //void calculateRemainingSeconds();

Q_SIGNALS:
    void exitLoop();

protected Q_SLOTS:
    //void slotEntries( KIO::Job*, const KIO::UDSEntryList& lst );
    void slotGetResult(KJob *);
    void slotDataReq(KIO::Job *, QByteArray &);
    void slotResult(KJob *);
    void slotMimetype(KIO::Job *, const QString &);

    void slotFileJobData(KIO::Job *job, const QByteArray &data);
    void slotFileJobRedirection(KIO::Job *job, const QUrl &url);
    void slotFileJobMimetype(KIO::Job *job, const QString &type);
    void slotFileJobOpen(KIO::Job *job);
    void slotFileJobWritten(KIO::Job *job, KIO::filesize_t written);
    void slotFileJobPosition(KIO::Job *job, KIO::filesize_t offset);
    void slotFileJobClose(KIO::Job *job);

    void slotFileJob2Data(KIO::Job *job, const QByteArray &data);
    void slotFileJob2Redirection(KIO::Job *job, const QUrl &url);
    void slotFileJob2Mimetype(KIO::Job *job, const QString &type);
    void slotFileJob2Open(KIO::Job *job);
    void slotFileJob2Written(KIO::Job *job, KIO::filesize_t written);
    void slotFileJob2Position(KIO::Job *job, KIO::filesize_t offset);

    void slotFileJob3Open(KIO::Job *job);
    void slotFileJob3Position(KIO::Job *job, KIO::filesize_t offset);
    void slotFileJob3Data(KIO::Job *job, const QByteArray &data);

    void slotFileJob4Open(KIO::Job *job);
    void slotFileJob4Truncated(KIO::Job *job, KIO::filesize_t length);

private:
    void enterLoop();
    enum { AlreadyExists = 1 };

    int m_result;
    bool m_closeSignalCalled;
    QFile m_truncatedFile;
    QByteArray m_data;
    QStringList m_names;
    int m_dataReqCount;
    QString m_mimetype;

    // openReadWrite test
    KIO::FileJob *fileJob;
    int m_rwCount;
};

#endif
