/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef TRASHIMPL_H
#define TRASHIMPL_H

#include <kio/job.h>
#include <KConfig>

#include <QDateTime>
#include <QMap>
#include <assert.h>

namespace Solid {
    class Device;
}

/**
 * Implementation of all low-level operations done by kio_trash
 * The structure of the trash directory follows the freedesktop.org standard <TODO URL>
 */
class TrashImpl : public QObject
{
    Q_OBJECT
public:
    TrashImpl();

    /// Check the "home" trash directory
    /// This MUST be called before doing anything else
    bool init();

    /// Create info for a file to be trashed
    /// Returns trashId and fileId
    /// The caller is then responsible for actually trashing the file
    bool createInfo(const QString &origPath, int &trashId, QString &fileId);

    /// Delete info file for a file to be trashed
    /// Usually used for undoing what createInfo did if trashing failed
    bool deleteInfo(int trashId, const QString &fileId);

    /// Moving a file or directory into the trash. The ids come from createInfo.
    bool moveToTrash(const QString &origPath, int trashId, const QString &fileId);

    /// Moving a file or directory out of the trash. The ids come from createInfo.
    bool moveFromTrash(const QString &origPath, int trashId, const QString &fileId, const QString &relativePath);

    /// Copying a file or directory into the trash. The ids come from createInfo.
    bool copyToTrash(const QString &origPath, int trashId, const QString &fileId);

    /// Copying a file or directory out of the trash. The ids come from createInfo.
    bool copyFromTrash(const QString &origPath, int trashId, const QString &fileId, const QString &relativePath);

    /// Renaming a file or directory in the trash.
    bool moveInTrash(int trashId, const QString &oldFileId, const QString &newFileId);

    /// Create a top-level trashed directory
    //bool mkdir( int trashId, const QString& fileId, int permissions );

    /// Get rid of a trashed file
    bool del(int trashId, const QString &fileId);

    /// Empty trash, i.e. delete all trashed files
    bool emptyTrash();

    /// Return true if the trash is empty
    bool isEmpty() const;

    struct TrashedFileInfo {
        int trashId; // for the url
        QString fileId; // for the url
        QString physicalPath; // for stat'ing etc.
        QString origPath; // from info file
        QDateTime deletionDate; // from info file
    };
    /// List trashed files
    typedef QList<TrashedFileInfo> TrashedFileInfoList;
    /// Returns the TrashedFileInfo of all files in all trashes
    /// uses scanTrashDirectories() to refresh m_trashDirectories
    TrashedFileInfoList list();

    /// Return the info for a given trashed file
    bool infoForFile(int trashId, const QString &fileId, TrashedFileInfo &info);

    struct TrashSpaceInfo {
        qulonglong totalSize; // total trash size in bytes
        qulonglong availableSize; // available trash space in bytes
    };
    /// Get the space info for a given trash path
    /// Space information is only valid if trashSpaceInfo returns true
    bool trashSpaceInfo(const QString &path, TrashSpaceInfo &info);

    /// Returns an UDSEntry corresponding to trash:/
    KIO::UDSEntry trashUDSEntry(KIO::StatDetails details);

    /// Return the physicalPath for a given trashed file - helper method which
    /// encapsulates the call to infoForFile. Don't use if you need more info from TrashedFileInfo.
    QString physicalPath(int trashId, const QString &fileId, const QString &relativePath);

    /// Move data from the old trash system to the new one
    void migrateOldTrash();

    /// KIO error code
    int lastErrorCode() const
    {
        return m_lastErrorCode;
    }
    QString lastErrorMessage() const
    {
        return m_lastErrorMessage;
    }

    QStringList listDir(const QString &physicalPath);

    static QUrl makeURL(int trashId, const QString &fileId, const QString &relativePath);
    static bool parseURL(const QUrl &url, int &trashId, QString &fileId, QString &relativePath);

    typedef QMap<int, QString> TrashDirMap;
    /// @internal This method is for TestTrash only. Home trash is included (id 0).
    TrashDirMap trashDirectories() const;
    /// @internal This method is for TestTrash only. No entry with id 0.
    TrashDirMap topDirectories() const;

Q_SIGNALS:
    void leaveModality();

private:
    /// Helper method. Moves a file or directory using the appropriate method.
    bool move(const QString &src, const QString &dest);
    bool copy(const QString &src, const QString &dest);
    /// Helper method. Tries to call ::rename(src,dest) and does error handling.
    bool directRename(const QString &src, const QString &dest);

    void fileAdded();
    void fileRemoved();

    bool adaptTrashSize(const QString &origPath, int trashId);

    // Warning, returns error code, not a bool
    int testDir(const QString &name) const;
    void error(int e, const QString &s);

    bool readInfoFile(const QString &infoPath, TrashedFileInfo &info, int trashId);

    QString infoPath(int trashId, const QString &fileId) const;
    QString filesPath(int trashId, const QString &fileId) const;

#ifdef Q_OS_OSX
    int idForMountPoint(const QString &mountPoint) const;
#else
    int idForDevice(const Solid::Device &device) const;
#endif
    void refreshDevices() const;

    /// Find the trash dir to use for a given file to delete, based on original path
    int findTrashDirectory(const QString &origPath);

    QString trashDirectoryPath(int trashId) const;
    QString topDirectoryPath(int trashId) const;

    bool synchronousDel(const QString &path, bool setLastErrorCode, bool isDir);

    void scanTrashDirectories() const;

    int idForTrashDirectory(const QString &trashDir) const;
    bool initTrashDirectory(const QByteArray &trashDir_c) const;
    bool checkTrashSubdirs(const QByteArray &trashDir_c) const;
    QString trashForMountPoint(const QString &topdir, bool createIfNeeded) const;
    static QString makeRelativePath(const QString &topdir, const QString &path);

    void enterLoop();

private Q_SLOTS:
    void jobFinished(KJob *job);

private:
    // delete the files and info subdirectories from all known trash directories
    // (supposed to be empty!) to make sure OS X sees the trash as empty too.
    // Stub except on OS X.
    void deleteEmptyTrashInfrastructure();
    // create the trash infrastructure; also called
    // to recreate it on OS X.
    bool createTrashInfrastructure(int trashId, const QString &path = QString());

    /// Last error code stored in class to simplify API.
    /// Note that this means almost no method can be const.
    int m_lastErrorCode;
    QString m_lastErrorMessage;

    enum { InitToBeDone, InitOK, InitError } m_initStatus;

    // A "trash directory" is a physical directory on disk,
    // e.g. $HOME/.local/share/Trash/$uid or /mnt/foo/.Trash/$uid
    // It has an id (number) and a path.
    // The home trash has id 0.
    mutable TrashDirMap m_trashDirectories; // id -> path of trash directory
    mutable TrashDirMap m_topDirectories; // id -> $topdir of partition
    dev_t m_homeDevice;
    mutable bool m_trashDirectoriesScanned;

    mutable KConfig m_config;

    // We don't cache any data related to the trashed files.
    // Another kioslave could change that behind our feet.
    // If we want to start caching data - and avoiding some race conditions -,
    // we should turn this class into a kded module and use DCOP to talk to it
    // from the kioslave.
};

#endif
