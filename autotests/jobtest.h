/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>

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

#ifndef JOBTEST_H
#define JOBTEST_H

#include <QString>
#include <QObject>
#include <kio/job.h>

class JobTest : public QObject
{
    Q_OBJECT

public:
    JobTest() {}

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    // Local tests (kio_file only)
    void storedGet();
    void put();
    void storedPut();
    void storedPutIODevice();
    void storedPutIODeviceFile();
    void storedPutIODeviceTempFile();
    void storedPutIODeviceFastDevice();
    void storedPutIODeviceSlowDevice();
    void storedPutIODeviceSlowDeviceBigChunk();
    void asyncStoredPutReadyReadAfterFinish();
    void copyFileToSamePartition();
    void copyDirectoryToSamePartition();
    void copyDirectoryToExistingDirectory();
    void copyDirectoryToExistingSymlinkedDirectory();
    void copyFileToOtherPartition();
    void copyDirectoryToOtherPartition();
    void copyRelativeSymlinkToSamePartition();
    void copyAbsoluteSymlinkToOtherPartition();
    void copyFolderWithUnaccessibleSubfolder();
    void copyDataUrl();
    void suspendFileCopy();
    void suspendCopy();
    void listRecursive();
    void listFile();
    void killJob();
    void killJobBeforeStart();
    void deleteJobBeforeStart();
    void directorySize();
    void directorySizeError();
    void moveFileToSamePartition();
    void moveDirectoryToSamePartition();
    void moveDirectoryIntoItself();
    void moveFileToOtherPartition();
    void moveSymlinkToOtherPartition();
    void moveDirectoryToOtherPartition();
    void moveFileNoPermissions();
    void moveDirectoryNoPermissions();
    void moveDirectoryToReadonlyFilesystem_data();
    void moveDirectoryToReadonlyFilesystem();
    void deleteFile();
    void deleteDirectory();
    void deleteSymlink();
    void deleteManyDirs();
    void deleteManyFilesIndependently();
    void deleteManyFilesTogether();
    void rmdirEmpty();
    void rmdirNotEmpty();
    void stat();
    void statDetailsBasic();
    void statDetailsBasicSetDetails();
    void statWithInode();
#ifndef Q_OS_WIN
    void statSymlink();
#endif
    void mostLocalUrl();
    void chmodFile();
#ifdef Q_OS_UNIX
    void chmodSticky();
#endif
    void chmodFileError();
    void mimeType();
    void mimeTypeError();
    void calculateRemainingSeconds();
    void moveFileDestAlreadyExists();
    void moveDestAlreadyExistsAutoRename_data();
    void moveDestAlreadyExistsAutoRename();

    void copyDirectoryAlreadyExistsSkip();
    void copyFileAlreadyExistsRename();

    void safeOverwrite();
    void safeOverwrite_data();
    void moveAndOverwrite();
    void moveOverSymlinkToSelf();
    void createSymlink();
    void createSymlinkTargetDirDoesntExist();
    void createSymlinkAsShouldSucceed();
    void createSymlinkAsShouldFailDirectoryExists();
    void createSymlinkAsShouldFailFileExists();
    void createSymlinkWithOverwriteShouldWork();
    void createBrokenSymlink();

    void cancelCopyAndCleanDest();
    void cancelCopyAndCleanDest_data();

    // Remote tests
    //void copyFileToSystem();

    void getInvalidUrl();
    void multiGet();

Q_SIGNALS:
    void exitLoop();

protected Q_SLOTS:
    void slotEntries(KIO::Job *, const KIO::UDSEntryList &lst);
    void slotGetResult(KJob *);
    void slotDataReq(KIO::Job *, QByteArray &);
    void slotResult(KJob *);
    void slotMimetype(KIO::Job *, const QString &);

private:
    void enterLoop();
    enum { AlreadyExists = 1 };
    void copyLocalFile(const QString &src, const QString &dest);
    void copyLocalDirectory(const QString &src, const QString &dest, int flags = 0);
    void moveLocalFile(const QString &src, const QString &dest);
    void moveLocalDirectory(const QString &src, const QString &dest);
    //void copyFileToSystem( bool resolve_local_urls );
    void deleteSymlink(bool using_fast_path);
    void deleteManyDirs(bool using_fast_path);
    void deleteManyFilesTogether(bool using_fast_path);
    void moveDestAlreadyExistsAutoRename(const QString &destDir, bool moveDirs);

    int m_result;
    QByteArray m_data;
    QStringList m_names;
    int m_dataReqCount;
    QString m_mimetype;
};

#endif
