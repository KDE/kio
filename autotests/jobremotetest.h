/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>
   Copyright (C) 2008 Norbert Frese <nf2@scheinwelt.at>

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

/*
   Please set KIO_JOBREMOTETEST_REMOTETMP to test other protocols than kio_file.
   Don't forget the trailing slash!
*/

#ifndef JOBTEST_H
#define JOBTEST_H

#include <QtCore/QString>
#include <QtCore/QObject>
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

    //void calculateRemainingSeconds();

Q_SIGNALS:
    void exitLoop();

protected Q_SLOTS:
    //void slotEntries( KIO::Job*, const KIO::UDSEntryList& lst );
    void slotGetResult( KJob* );
    void slotDataReq( KIO::Job*, QByteArray& );
    void slotResult( KJob* );
    void slotMimetype(KIO::Job*, const QString&);

    void slotFileJobData (KIO::Job *job, const QByteArray &data);
    void slotFileJobRedirection (KIO::Job *job, const QUrl &url);
    void slotFileJobMimetype (KIO::Job *job, const QString &type);
    void slotFileJobOpen (KIO::Job *job);
    void slotFileJobWritten (KIO::Job *job, KIO::filesize_t written);
    void slotFileJobPosition (KIO::Job *job, KIO::filesize_t offset);
    void slotFileJobClose (KIO::Job *job);

    void slotFileJob2Data (KIO::Job *job, const QByteArray &data);
    void slotFileJob2Redirection (KIO::Job *job, const QUrl &url);
    void slotFileJob2Mimetype (KIO::Job *job, const QString &type);
    void slotFileJob2Open (KIO::Job *job);
    void slotFileJob2Written (KIO::Job *job, KIO::filesize_t written);
    void slotFileJob2Position (KIO::Job *job, KIO::filesize_t offset);
    void slotFileJob2Close (KIO::Job *job);


private:
    void enterLoop();
    enum { AlreadyExists = 1 };

    int m_result;
    QByteArray m_data;
    QStringList m_names;
    int m_dataReqCount;
    QString m_mimetype;

    // openReadWrite test
    KIO::FileJob * fileJob;
    int m_rwCount;
};

#endif
