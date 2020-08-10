/*
    SPDX-FileCopyrightText: 2013 Mark Gaiser <markg85@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef LISTDIRTEST_H
#define LISTDIRTEST_H

#include <QObject>
#include <kio/job.h>

class ListDirTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void numFilesTestCase_data();
    void numFilesTestCase();

    void slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries);

private:
    void createEmptyTestFiles(int numOfFilesToCreate, const QString &path);
    int m_receivedEntryCount;
};

#endif
