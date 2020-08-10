/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Kevin Ottens <ervin@ipsquad.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _FORWARDING_SLAVE_BASE_H_
#define _FORWARDING_SLAVE_BASE_H_

#include "kiocore_export.h"
#include <kio/slavebase.h>
#include "job_base.h" // JobFlags

#include <QObject>
#include <QEventLoop>

namespace KIO
{

class ForwardingSlaveBasePrivate;

/**
 * @class KIO::ForwardingSlaveBase forwardingslavebase.h <KIO/ForwardingSlaveBase>
 *
 * This class should be used as a base for ioslaves acting as a
 * forwarder to other ioslaves. It has been designed to support only
 * local filesystem like ioslaves.
 *
 * If the resulting ioslave should be a simple proxy, you only need
 * to implement the ForwardingSlaveBase::rewriteUrl() method.
 *
 * For more advanced behavior, the classic ioslave methods should
 * be reimplemented, because their default behavior in this class
 * is to forward using the ForwardingSlaveBase::rewriteUrl() method.
 *
 * A possible code snippet for an advanced stat() behavior would look
 * like this in the child class:
 *
 * \code
 *     void ChildProtocol::stat(const QUrl &url)
 *     {
 *         bool is_special = false;
 *
 *         // Process the URL to see if it should have
 *         // a special treatment
 *
 *         if ( is_special )
 *         {
 *             // Handle the URL ourselves
 *             KIO::UDSEntry entry;
 *             // Fill entry with UDSAtom instances
 *             statEntry(entry);
 *             finished();
 *         }
 *         else
 *         {
 *             // Setup the ioslave internal state if
 *             // required by ChildProtocol::rewriteUrl()
 *             ForwardingSlaveBase::stat(url);
 *         }
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
 * This class was initially used for media:/ ioslave. This ioslave code
 * and the MediaDirNotify class of its companion kded module can be a
 * good source of inspiration.
 *
 * @see ForwardingSlaveBase::rewriteUrl()
 * @author Kevin Ottens <ervin@ipsquad.net>
 */
class KIOCORE_EXPORT ForwardingSlaveBase : public QObject, public SlaveBase
{
    Q_OBJECT
public:
    ForwardingSlaveBase(const QByteArray &protocol,
                        const QByteArray &poolSocket,
                        const QByteArray &appSocket);
    ~ForwardingSlaveBase() override;

    void get(const QUrl &url) override;

    void put(const QUrl &url, int permissions,
                     JobFlags flags) override;

    void stat(const QUrl &url) override;

    void mimetype(const QUrl &url) override;

    void listDir(const QUrl &url) override;

    void mkdir(const QUrl &url, int permissions) override;

    void rename(const QUrl &src, const QUrl &dest, JobFlags flags) override;

    void symlink(const QString &target, const QUrl &dest,
                         JobFlags flags) override;

    void chmod(const QUrl &url, int permissions) override;

    void setModificationTime(const QUrl &url, const QDateTime &mtime) override;

    void copy(const QUrl &src, const QUrl &dest,
                      int permissions, JobFlags flags) override;

    void del(const QUrl &url, bool isfile) override;

protected:
    /**
     * Rewrite an url to its forwarded counterpart. It should return
     * true if everything was ok, and false otherwise.
     *
     * If a problem is detected it's up to this method to trigger error()
     * before returning. Returning false silently cancels the current
     * slave operation.
     *
     * @param url The URL as given during the slave call
     * @param newURL The new URL to forward the slave call to
     * @return true if the given url could be correctly rewritten
     */
    virtual bool rewriteUrl(const QUrl &url, QUrl &newURL) = 0;

    /**
     * Allow to modify a UDSEntry before it's sent to the ioslave endpoint.
     * This is the default implementation working in most cases, but sometimes
     * you could make use of more forwarding black magic (for example
     * dynamically transform any desktop file into a fake directory...)
     *
     * @param entry the UDSEntry to post-process
     * @param listing indicate if this entry it created during a listDir
     *                operation
     */
    virtual void prepareUDSEntry(KIO::UDSEntry &entry,
                                 bool listing = false) const;

    /**
     * Return the URL being processed by the ioslave
     * Only access it inside prepareUDSEntry()
     */
    QUrl processedUrl() const;

    /**
     * Return the URL asked to the ioslave
     * Only access it inside prepareUDSEntry()
     */
    QUrl requestedUrl() const;

private:
    // KIO::Job
    Q_PRIVATE_SLOT(d, void _k_slotResult(KJob *job))
    Q_PRIVATE_SLOT(d, void _k_slotWarning(KJob *job, const QString &msg))
    Q_PRIVATE_SLOT(d, void _k_slotInfoMessage(KJob *job, const QString &msg))
    Q_PRIVATE_SLOT(d, void _k_slotTotalSize(KJob *job, qulonglong size))
    Q_PRIVATE_SLOT(d, void _k_slotProcessedSize(KJob *job, qulonglong size))
    Q_PRIVATE_SLOT(d, void _k_slotSpeed(KJob *job, unsigned long bytesPerSecond))

    // KIO::SimpleJob subclasses
    Q_PRIVATE_SLOT(d, void _k_slotRedirection(KIO::Job *job, const QUrl &url))

    // KIO::ListJob
    Q_PRIVATE_SLOT(d, void _k_slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries))

    // KIO::TransferJob
    Q_PRIVATE_SLOT(d, void _k_slotData(KIO::Job *job, const QByteArray &data))
    Q_PRIVATE_SLOT(d, void _k_slotDataReq(KIO::Job *job, QByteArray &data))
    Q_PRIVATE_SLOT(d, void _k_slotMimetype(KIO::Job *job, const QString &type))
    Q_PRIVATE_SLOT(d, void _k_slotCanResume(KIO::Job *job, KIO::filesize_t offset))

    friend class ForwardingSlaveBasePrivate;
    ForwardingSlaveBasePrivate *const d;
};

}

#endif
