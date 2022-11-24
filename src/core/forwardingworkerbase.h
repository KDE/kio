/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Kevin Ottens <ervin@ipsquad.net>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _FORWARDING_WORKER_BASE_H_
#define _FORWARDING_WORKER_BASE_H_

#include "kiocore_export.h"
#include <kio/workerbase.h>

#include <QObject>

#include <memory>

namespace KIO
{
class ForwardingWorkerBasePrivate;

/**
 * @class KIO::ForwardingWorkerBase forwardingworkerbase.h <KIO/ForwardingWorkerBase>
 *
 * This class should be used as a base for KIO workers acting as a
 * forwarder to other KIO workers. It has been designed to support only
 * local filesystem like KIO workers.
 *
 * If the resulting KIO worker should be a simple proxy, you only need
 * to implement the ForwardingWorkerBase::rewriteUrl() method.
 *
 * For more advanced behavior, the classic KIO worker methods should
 * be reimplemented, because their default behavior in this class
 * is to forward using the ForwardingWorkerBase::rewriteUrl() method.
 *
 * A possible code snippet for an advanced stat() behavior would look
 * like this in the child class:
 *
 * \code
 *     WorkerResult ChildProtocol::stat(const QUrl &url)
 *     {
 *         bool is_special = false;
 *
 *         // Process the URL to see if it should have
 *         // a special treatment
 *
 *         if (is_special) {
 *             // Handle the URL ourselves
 *             KIO::UDSEntry entry;
 *             // Fill entry with values
 *             statEntry(entry);
 *             return WorkerResult::pass();
 *         }
 *         // Setup the KIO worker internal state if
 *         // required by ChildProtocol::rewriteUrl()
 *         return ForwardingWorkerBase::stat(url);
 *     }
 * \endcode
 *
 * Of course in this case, you surely need to reimplement listDir()
 * and get() accordingly.
 *
 * If you want view on directories to be correctly refreshed when
 * something changes on a forwarded URL, you'll need a companion kded
 * module to emit the KDirNotify Files*() D-Bus signals.
 *
 * @see ForwardingWorkerBase::rewriteUrl()
 * @author Kevin Ottens <ervin@ipsquad.net>
 * @since 5.101
 */
class KIOCORE_EXPORT ForwardingWorkerBase : public QObject, public WorkerBase
{
    Q_OBJECT
public:
    ForwardingWorkerBase(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket);
    ~ForwardingWorkerBase() override;
    Q_DISABLE_COPY_MOVE(ForwardingWorkerBase)

    WorkerResult get(const QUrl &url) override;
    WorkerResult put(const QUrl &url, int permissions, JobFlags flags) override;
    WorkerResult stat(const QUrl &url) override;
    WorkerResult mimetype(const QUrl &url) override;
    WorkerResult listDir(const QUrl &url) override;
    WorkerResult mkdir(const QUrl &url, int permissions) override;
    WorkerResult rename(const QUrl &src, const QUrl &dest, JobFlags flags) override;
    WorkerResult symlink(const QString &target, const QUrl &dest, JobFlags flags) override;
    WorkerResult chmod(const QUrl &url, int permissions) override;
    WorkerResult setModificationTime(const QUrl &url, const QDateTime &mtime) override;
    WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, JobFlags flags) override;
    WorkerResult del(const QUrl &url, bool isfile) override;

protected:
    /**
     * Rewrite an url to its forwarded counterpart. It should return
     * true if everything was ok, and false otherwise.
     *
     * If a problem is detected it's up to this method to trigger error()
     * before returning. Returning false silently cancels the current
     * KIO worker operation.
     *
     * @param url The URL as given during the KIO worker call
     * @param newURL The new URL to forward the KIO worker call to
     * @return true if the given url could be correctly rewritten
     */
    virtual bool rewriteUrl(const QUrl &url, QUrl &newURL) = 0;

    enum UDSEntryCreationMode {
        UDSEntryCreationInStat, ///< The entry is created during a stat operation.
        UDSEntryCreationInListDir, ///<  The entry is created during a listDir operation.
    };

    /**
     * Adjusts a UDSEntry before it's sent in the reply to the KIO worker endpoint.
     * This is the default implementation working in most cases, but sometimes
     * you could make use of more forwarding black magic (for example
     * dynamically transform any desktop file into a fake directory...)
     *
     * @param entry the UDSEntry to adjust
     * @param creationMode the operation for which this entry is created
     */
    virtual void adjustUDSEntry(KIO::UDSEntry &entry, UDSEntryCreationMode creationMode) const;

    /**
     * Return the URL being processed by the KIO worker
     * Only access it inside adjustUDSEntry()
     */
    QUrl processedUrl() const;

    /**
     * Return the URL asked to the KIO worker
     * Only access it inside adjustUDSEntry()
     */
    QUrl requestedUrl() const;

private:
    friend class ForwardingWorkerBasePrivate;
    std::unique_ptr<ForwardingWorkerBasePrivate> const d;
};

} // namespace KIO

#endif
