/*
 *  Copyright (C) 2013 Mark Gaiser <markg85@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */
#ifndef LISTDIRTEST_H
#define LISTDIRTEST_H

#include <QObject>
#include <kio/job.h>

class ListDirTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void numFilesTestCase_data();
    void numFilesTestCase();

    void slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries);

private:
    void createEmptyTestFiles(int numOfFilesToCreate, const QString& path);
    int m_receivedEntryCount;
};

#endif
