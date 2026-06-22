/*
    SPDX-FileCopyrightText: 2000-2002 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __file_h__
#define __file_h__

#include <kio/global.h>
#include <kio/workerbase.h>

#include <KUser>
#include <QFile>
#include <QHash>
#include <QObject>

#include <config-kioworker-file.h>
#include <qplatformdefs.h> // mode_t

#if HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif
#if HAVE_ACL_LIBACL_H
#include <acl/libacl.h>
#endif

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KIO_FILE)

class QDataStream;

class FileProtocol : public QObject, public KIO::WorkerBase
{
    Q_OBJECT
public:
    FileProtocol(const QByteArray &pool, const QByteArray &app);
    ~FileProtocol() override;

    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult put(const QUrl &url, int _mode, KIO::JobFlags _flags) override;
    KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int mode, KIO::JobFlags flags) override;
    KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;
    KIO::WorkerResult symlink(const QString &target, const QUrl &dest, KIO::JobFlags flags) override;

    KIO::WorkerResult stat(const QUrl &url) override;
    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult mkdir(const QUrl &url, int permissions) override;
    KIO::WorkerResult chmod(const QUrl &url, int permissions) override;
    KIO::WorkerResult chown(const QUrl &url, const QString &owner, const QString &group) override;
    KIO::WorkerResult setModificationTime(const QUrl &url, const QDateTime &mtime) override;
    KIO::WorkerResult del(const QUrl &url, bool isfile) override;
    KIO::WorkerResult open(const QUrl &url, QIODevice::OpenMode mode) override;
    KIO::WorkerResult read(KIO::filesize_t size) override;
    KIO::WorkerResult write(const QByteArray &data) override;
    KIO::WorkerResult seek(KIO::filesize_t offset) override;
    KIO::WorkerResult truncate(KIO::filesize_t length) override;
    bool copyXattrs(const int src_fd, const int dest_fd);
    KIO::WorkerResult close() override;

    KIO::WorkerResult fileSystemFreeSpace(const QUrl &url) override;

    /*!
     * Special commands supported by this worker:
     * 1 - mount
     * 2 - unmount
     */
    KIO::WorkerResult special(const QByteArray &data) override;
    KIO::WorkerResult unmount(const QString &point);
    KIO::WorkerResult mount(bool _ro, const char *_fstype, const QString &dev, const QString &point);
    // Batch-copy a list of local files in one command (special() sub-command). Platform-specific:
    // the real engine is in file_unix.cpp; file_win.cpp returns ERR_UNSUPPORTED_ACTION.
    KIO::WorkerResult batchCopy(QDataStream &stream);

#if HAVE_POSIX_ACL
    static bool isExtendedACL(acl_t acl);
#endif

private:
    int setACL(const char *path, mode_t perm, bool _directoryDefault);
    KIO::WorkerResult deleteRecursive(const QString &path);

#ifdef Q_OS_WIN
    // Set the modification time of path, leaving the creation and access times
    // unchanged. Returns false on failure.
    static bool setWindowsModificationTime(const QString &path, const QDateTime &mtime);
#endif

    KIO::WorkerResult redirect(const QUrl &url);

    // Returns true if @p url is a file:// URL that refers to this machine, i.e.
    // its host is empty, "localhost" or this host's name (RFC 8089). Such a host
    // must be stripped before QUrl::toLocalFile(), which would otherwise keep it
    // as a UNC-style "//host/path".
    static bool isLocalFileSameHost(const QUrl &url);

    // Returns @p url with the (local) host removed, so QUrl::toLocalFile() gives
    // a plain local path. A no-op for URLs whose host isn't the local machine.
    static QUrl localFileWithoutHostname(const QUrl &url);

    // Close without calling finish(). Use this to close after error.
    void closeWithoutFinish();

private:
    QFile *mFile;

    bool resultWasCancelled(KIO::WorkerResult result);

    bool testMode = false;
    KIO::StatDetails getStatDetails();
};

#endif
