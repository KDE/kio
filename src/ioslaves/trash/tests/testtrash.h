/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef TESTTRASH_H
#define TESTTRASH_H

#include <QObject>
#include <QTemporaryDir>

#include <KIO/Job>

class TestTrash : public QObject
{
    Q_OBJECT

public:
    TestTrash() {}

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testIcons();

    void urlTestFile();
    void urlTestDirectory();
    void urlTestSubDirectory();

    void trashFileFromHome();
    void trashPercentFileFromHome();
    void trashUtf8FileFromHome();
    void trashUmlautFileFromHome();
    void testTrashNotEmpty();
    void trashFileFromOther();
    void trashFileIntoOtherPartition();
    void trashFileOwnedByRoot();
    void trashSymlinkFromHome();
    void trashSymlinkFromOther();
    void trashBrokenSymlinkFromHome();
    void trashDirectoryFromHome();
    void trashDotDirectory();
    void trashReadOnlyDirFromHome();
    void trashDirectoryFromOther();
    void trashDirectoryOwnedByRoot();
    void trashDirectoryWithTrailingSlash();
    void trashBrokenSymlinkIntoSubdir();

    void statRoot();
    void statFileInRoot();
    void statDirectoryInRoot();
    void statSymlinkInRoot();
    void statFileInDirectory();
    void statBrokenSymlinkInSubdir();
    void testRemoveStaleInfofile();

    void copyFileFromTrash();
    void copyFileInDirectoryFromTrash();
    void copyDirectoryFromTrash();
    void copySymlinkFromTrash();

    void renameFileInTrash();
    void renameDirInTrash();
    void moveFileFromTrash();
    void moveFileFromTrashToDir_data();
    void moveFileFromTrashToDir();
    void moveFileInDirectoryFromTrash();
    void moveDirectoryFromTrash();
    void moveSymlinkFromTrash();
    void testMoveNonExistingFile();

    void listRootDir();
    void listRecursiveRootDir();
    void mostLocalUrlTest();
    void listSubDir();

    void delRootFile();
    void delFileInDirectory();
    void delDirectory();

    void getFile();
    void restoreFile();
    void restoreFileFromSubDir();
    void restoreFileToDeletedDirectory();

    void emptyTrash();
    void testEmptyTrashSize();

protected Q_SLOTS:
    void slotEntries(KIO::Job *, const KIO::UDSEntryList &);

private:
    void trashFile(const QString &origFilePath, const QString &fileId);
    void trashSymlink(const QString &origFilePath, const QString &fileName, bool broken);
    void trashDirectory(const QString &origPath, const QString &fileName);
    void copyFromTrash(const QString &fileId, const QString &destPath, const QString &relativePath = QString());
    void moveInTrash(const QString &fileId, const QString &destFileId);
    void moveFromTrash(const QString &fileId, const QString &destPath, const QString &relativePath = QString());
    void checkDirCacheValidity();

    QString homeTmpDir() const;
    QString otherTmpDir() const;
    QString utf8FileName() const;
    QString umlautFileName() const;
    QString readOnlyDirPath() const;

    QString m_trashDir;

    QString m_otherPartitionTopDir;
    QString m_otherPartitionTrashDir;
    bool m_tmpIsWritablePartition;
    int m_tmpTrashId;
    int m_otherPartitionId;

    int m_entryCount;
    QStringList m_listResult;
    QStringList m_displayNameListResult;

    QTemporaryDir m_tempDir;
};

#endif
