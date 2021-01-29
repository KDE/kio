/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef MULTIGETJOB_H
#define MULTIGETJOB_H

#include "transferjob.h"

namespace KIO
{

class MultiGetJobPrivate;
/**
 * @class KIO::MultiGetJob multigetjob.h <KIO/MultiGetJob>
 *
 * The MultiGetJob is a TransferJob that allows you to get
 * several files from a single server. Don't create directly,
 * but use KIO::multi_get() instead.
 * @see KIO::multi_get()
 */
class KIOCORE_EXPORT MultiGetJob : public TransferJob
{
    Q_OBJECT

public:
    ~MultiGetJob() override;

    /**
     * Get an additional file.
     *
     * @param id the id of the file
     * @param url the url of the file to get
     * @param metaData the meta data for this request
     */
    void get(long id, const QUrl &url, const MetaData &metaData);

Q_SIGNALS:
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Data from the slave has arrived.
     *
     * @param id the id of the request
     * @param data data received from the slave.
     * End of data (EOD) has been reached if data.size() == 0
     *
     * @deprecated since 5.79, use KIO::MultiGetJob::dataReceived(long, const QByteArray &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use KIO::MultiGetJob::dataReceived(long, const QByteArray &)")
    void data(long id, const QByteArray &data);
#endif

    /**
     * Data from the slave has arrived.
     *
     * @param id the id of the request
     * @param data data received from the slave.
     * End of data (EOD) has been reached if data.size() == 0
     *
     * @since 5.79
     */
    void dataReceived(long id, const QByteArray &data);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 78)
    /**
     * MIME type determined
     * @param id the id of the request
     * @param mimeType the MIME type
     * @deprecated Since 5.78, use mimeTypeFound(KIO::Job *, const QString &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 78, "Use KIO::MultiGetJob::mimeTypeFound(long id, const QString &)")
    void mimetype(long id, const QString &mimeType);
#endif

    /**
     * MIME type determined
     * @param id the id of the request
     * @param mimeType the MIME type
     * @since 5.78
     */
    void mimeTypeFound(long id, const QString &mimeType);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * File transfer completed.
     *
     * When all files have been processed, result(KJob *) gets
     * emitted.
     * @param id the id of the request
     *
     * @deprecated since 5.79, use KIO::MultiGetJob::fileTransferred(long)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use KIO::MultiGetJob::fileTransferred(long id)")
    void result(long id);
#endif

    /**
     * File transfer completed.
     *
     * When all files have been processed, result(KJob *) gets emitted.
     *
     * @param id the id of the request
     *
     * @since 5.79
     */
    void fileTransferred(long id);

protected Q_SLOTS:
    void slotRedirection(const QUrl &url) override;
    void slotFinished() override;
    void slotData(const QByteArray &data) override;
    void slotMimetype(const QString &mimetype) override;

protected:
    MultiGetJob(MultiGetJobPrivate &dd);
private:
    Q_DECLARE_PRIVATE(MultiGetJob)
};

/**
 * Creates a new multiple get job.
 *
 * @param id the id of the get operation
 * @param url the URL of the file
 * @param metaData the MetaData associated with the file
 *
 * @return the job handling the operation.
 * @see get()
 */
KIOCORE_EXPORT MultiGetJob *multi_get(long id, const QUrl &url, const MetaData &metaData);

}

#endif
