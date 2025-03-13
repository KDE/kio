/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_STOREDTRANSFERJOB
#define KIO_STOREDTRANSFERJOB

#include "transferjob.h"

namespace KIO
{
class StoredTransferJobPrivate;
/*!
 * \class KIO::StoredTransferJob
 * \inheaderfile KIO/StoredTransferJob
 * \inmodule KIOCore
 *
 * \brief StoredTransferJob is a TransferJob (for downloading or uploading data) that
 * also stores a QByteArray with the data, making it simpler to use than the
 * standard TransferJob.
 *
 * For KIO::storedGet it puts the data into the member QByteArray, so the user
 * of this class can get hold of the whole data at once by calling data()
 * when the result signal is emitted.
 * You should only use StoredTransferJob to download data if you cannot
 * process the data by chunks while it's being downloaded, since storing
 * everything in a QByteArray can potentially require a lot of memory.
 *
 * For KIO::storedPut the user of this class simply provides the bytearray from
 * the start, and the job takes care of uploading it.
 * You should only use StoredTransferJob to upload data if you cannot
 * provide the in chunks while it's being uploaded, since storing
 * everything in a QByteArray can potentially require a lot of memory.
 */
class KIOCORE_EXPORT StoredTransferJob : public KIO::TransferJob
{
    Q_OBJECT

public:
    ~StoredTransferJob() override;

    /*!
     * Set data to be uploaded. This is for put jobs.
     * Automatically called by KIO::storedPut(const QByteArray &, ...),
     * do not call this yourself.
     */
    void setData(const QByteArray &arr);

    /*!
     * Get hold of the downloaded data. This is for get jobs.
     * You're supposed to call this only from the slot connected to the result() signal.
     */
    QByteArray data() const;

protected:
    KIOCORE_NO_EXPORT explicit StoredTransferJob(StoredTransferJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(StoredTransferJob)
};

/*!
 * \relates KIO::StoredTransferJob
 *
 * Get (means: read), into a single QByteArray.
 *
 * \a url the URL of the file
 *
 * \a reload Reload to reload the file, NoReload if it can be taken from the cache
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT StoredTransferJob *storedGet(const QUrl &url, LoadType reload = NoReload, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::StoredTransferJob
 *
 * Put (means: write) data from a QIODevice.
 *
 * \a input The data to write, a device to read from. Must be open for reading (data will be read from the current position).
 *
 * \a url Where to write data.
 *
 * \a permissions May be -1. In this case no special permission mode is set.
 *
 * \a flags Can be HideProgressInfo, Overwrite and Resume here.
 * WARNING: Setting Resume means that the data will be appended to \a dest if \a dest exists.
 *
 * Returns the job handling the operation.
 *
 * \since 5.10
 */
KIOCORE_EXPORT StoredTransferJob *storedPut(QIODevice *input, const QUrl &url, int permissions, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::StoredTransferJob
 *
 * Put (means: write) data from a single QByteArray.
 *
 * \a arr The data to write
 *
 * \a url Where to write data.
 *
 * \a permissions May be -1. In this case no special permission mode is set.
 *
 * \a flags Can be HideProgressInfo, Overwrite and Resume here.
 * WARNING: Setting Resume means that the data will be appended to \a url if \a url exists.
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT StoredTransferJob *storedPut(const QByteArray &arr, const QUrl &url, int permissions, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::StoredTransferJob
 *
 * HTTP POST (means: write) data from a single QByteArray.
 *
 * \a arr The data to write
 *
 * \a url Where to write data.
 *
 * \a flags Can be HideProgressInfo here.
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT StoredTransferJob *storedHttpPost(const QByteArray &arr, const QUrl &url, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::StoredTransferJob
 *
 * HTTP POST (means: write) data from the given IO device.
 *
 * \a device Device from which the encoded data to be posted is read. Must be open for reading.
 *
 * \a url Where to write data.
 *
 * \a size Size of the encoded data to be posted.
 *
 * \a flags Can be HideProgressInfo here.
 *
 * Returns the job handling the operation.
 *
 */
KIOCORE_EXPORT StoredTransferJob *storedHttpPost(QIODevice *device, const QUrl &url, qint64 size = -1, JobFlags flags = DefaultFlags);

}

#endif
