/*
    SPDX-FileCopyrightText: 2000-2002 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __file_h__
#define __file_h__

#include <kio/global.h>
#include <kio/slavebase.h>

#include <KUser>
#include <QFile>
#include <QHash>
#include <QObject>

#include <config-kioslave-file.h>
#include <qplatformdefs.h> // mode_t

#if HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#if HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif

#include "file_p.h"

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KIO_FILE)

#ifdef HAVE_AUTH_HELPER
class OrgKdeKioFilemanagementInterface;
#endif

class FileProtocol : public QObject, public KIO::SlaveBase
{
    Q_OBJECT
public:
    FileProtocol(const QByteArray &pool, const QByteArray &app);
    ~FileProtocol() override;

    void get(const QUrl &url) override;
    virtual void put(const QUrl &url, int _mode, KIO::JobFlags _flags) override;
    virtual void copy(const QUrl &src, const QUrl &dest, int mode, KIO::JobFlags flags) override;
    virtual void rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;
    virtual void symlink(const QString &target, const QUrl &dest, KIO::JobFlags flags) override;

    void stat(const QUrl &url) override;
    void listDir(const QUrl &url) override;
    void mkdir(const QUrl &url, int permissions) override;
    void chmod(const QUrl &url, int permissions) override;
    void chown(const QUrl &url, const QString &owner, const QString &group) override;
    void setModificationTime(const QUrl &url, const QDateTime &mtime) override;
    void del(const QUrl &url, bool isfile) override;
    void open(const QUrl &url, QIODevice::OpenMode mode) override;
    void read(KIO::filesize_t size) override;
    void write(const QByteArray &data) override;
    void seek(KIO::filesize_t offset) override;
    void truncate(KIO::filesize_t length);
    bool copyXattrs(const int src_fd, const int dest_fd);
    void close() override;

    /**
     * Special commands supported by this slave:
     * 1 - mount
     * 2 - unmount
     */
    void special(const QByteArray &data) override;
    void unmount(const QString &point);
    void mount(bool _ro, const char *_fstype, const QString &dev, const QString &point);

#if HAVE_POSIX_ACL
    static bool isExtendedACL(acl_t acl);
#endif

protected:
    void virtual_hook(int id, void *data) override;

private:
    int setACL(const char *path, mode_t perm, bool _directoryDefault);
    QString getUserName(KUserId uid) const;
    QString getGroupName(KGroupId gid) const;
    bool deleteRecursive(const QString &path);

    void fileSystemFreeSpace(const QUrl &url); // KF6 TODO: Turn into virtual method in SlaveBase

    bool privilegeOperationUnitTestMode();
    PrivilegeOperationReturnValue tryOpen(QFile &f, const QByteArray &path, int flags, int mode, int errcode);

    void redirect(const QUrl &url);

    // Close without calling finish(). Use this to close after error.
    void closeWithoutFinish();

private:
    QFile *mFile;

    #ifdef HAVE_AUTH_HELPER
    OrgKdeKioFilemanagementInterface* mRootIFace;
    #endif

    bool testMode = false;
    KIO::StatDetails getStatDetails();
};

#endif
