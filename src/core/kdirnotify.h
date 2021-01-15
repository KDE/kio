/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2012 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2006 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KDIRNOTIFY_H
#define KDIRNOTIFY_H

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QDBusAbstractInterface>
#include "kiocore_export.h"

class QDBusConnection;

/**
 * \class OrgKdeKDirNotifyInterface kdirnotify.h KDirNotify
 *
 * \brief Proxy class for interface org.kde.KDirNotify.
 *
 * KDirNotify can be used to inform KIO about changes in real or virtual file systems.
 * Classes like KDirModel connect to the signals as in the following example to
 * be able to keep caches up-to-date.
 *
 * \code
 * kdirnotify = new org::kde::KDirNotify(QString(), QString(), QDBusConnection::sessionBus(), this);
 * connect(kdirnotify, &KDirNotify::FileRenamedWithLocalPath,
 *         this, [this](const QString &src, const QString &dst, const QString &dstPath) {
 *     slotFileRenamed(src, dst, dstPath);
 * });
 *
 * connect(kdirnotify, &KDirNotify::FilesAdded,
 *         this, [this](const QString &directory) { slotFilesAdded(directory); });
 *
 * connect(kdirnotify, &KDirNotify::FilesChanged,
 *         this, [this](const QStringList &fileList) { slotFilesChanged(fileList); });
 *
 * connect(kdirnotify, &KDirNotify::FilesRemoved,
 *         this, [this](const QStringList &fileList) { slotFilesRemoved(fileList); });
 * \endcode
 *
 * Especially noteworthy are the empty strings for both \p service and \p path. That
 * way the client will connect to signals emitted by any application.
 *
 * The second usage is to actually emit the signals. For that emitFileRenamed() and friends are
 * to be used.
 */
class KIOCORE_EXPORT OrgKdeKDirNotifyInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    {
        return "org.kde.KDirNotify";
    }

public:
    /**
     * Create a new KDirNotify interface.
     *
     * \param service The service whose signals one wants to listed to. Use an empty
     * string to connect to all services/applications.
     * \param path The path to the D-Bus object whose signals one wants to listed to.
     * Use an empty string to connect to signals from all objects.
     * \param connection Typically QDBusConnection::sessionBus().
     * \param parent The parent QObject.
     */
    OrgKdeKDirNotifyInterface(const QString &service, const QString &path, const QDBusConnection &connection = QDBusConnection::sessionBus(), QObject *parent = nullptr);

    /**
     * Destructor.
     */
    ~OrgKdeKDirNotifyInterface();

public Q_SLOTS: // METHODS
Q_SIGNALS: // SIGNALS
    void FileRenamed(const QString &src, const QString &dst);
    void FileRenamedWithLocalPath(const QString &src, const QString &dst, const QString &dstPath);
    void FileMoved(const QString &src, const QString &dst);
    void FilesAdded(const QString &directory);
    void FilesChanged(const QStringList &fileList);
    void FilesRemoved(const QStringList &fileList);
    void enteredDirectory(const QString &url);
    void leftDirectory(const QString &url);

public:
    static void emitFileRenamed(const QUrl &src, const QUrl &dst);
    /**
     * \param src The old URL of the file that has been renamed.
     * \param dst The new URL of the file after it was renamed.
     * \param dstPath The local path of the file after it was renamed. This may be empty
     * and should otherwise be used to update UDS_LOCAL_PATH.
     * @since 5.20
     */
    static void emitFileRenamedWithLocalPath(const QUrl &src, const QUrl &dst, const QString &dstPath);
    static void emitFileMoved(const QUrl &src, const QUrl &dst);
    static void emitFilesAdded(const QUrl &directory);
    static void emitFilesChanged(const QList<QUrl> &fileList);
    static void emitFilesRemoved(const QList<QUrl> &fileList);
    static void emitEnteredDirectory(const QUrl &url);
    static void emitLeftDirectory(const QUrl &url);
};

namespace org
{
namespace kde
{
typedef ::OrgKdeKDirNotifyInterface KDirNotify;
}
}
#endif
