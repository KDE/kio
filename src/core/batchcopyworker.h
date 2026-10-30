/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_BATCHCOPYWORKER_H
#define KIO_BATCHCOPYWORKER_H

#include "global.h" // filesize_t
#include "job_base.h" // JobFlags
#include "kiocore_export.h"
#include "workerbase.h"

#include <QDateTime>
#include <QList>
#include <QUrl>

namespace KIO
{

/*!
 * \class KIO::BatchCopyItem
 * \inheaderfile KIO/BatchCopyWorker
 * \inmodule KIOCore
 *
 * \brief One file of a batch copy.
 *
 * \since 6.32
 */
struct KIOCORE_EXPORT BatchCopyItem {
    /*!
     * The file to copy. Always a file, never a directory or a symlink.
     */
    QUrl source;

    /*!
     * Where the copy goes. The parent folder exists.
     */
    QUrl destination;

    /*!
     * The permissions of the copy, as for WorkerBase::copy(), or -1 to keep the default
     * permissions of the destination.
     */
    int permissions = -1;

    /*!
     * The modification time of the source, or an invalid QDateTime when it is unknown. The worker
     * gives it to the copy.
     */
    QDateTime modificationTime;

    /*!
     * The size of the source, or static_cast<KIO::filesize_t>(-1) when it is unknown.
     */
    KIO::filesize_t size = static_cast<KIO::filesize_t>(-1);

    bool operator==(const BatchCopyItem &other) const = default;
};

/*!
 * \class KIO::BatchCopyWorker
 * \inheaderfile KIO/BatchCopyWorker
 * \inmodule KIOCore
 *
 * \brief Copies a list of files in one worker command.
 *
 * Copying many small files is dominated by the cost of each file job, of the scheduler and of the
 * request and answer between the application and the worker. A worker that copies a list of files
 * in one command pays that cost once for the whole list.
 *
 * A worker implements this interface next to WorkerBase:
 *
 * \code
 * class SmbWorker : public KIO::WorkerBase, public KIO::BatchCopyWorker
 * {
 *     KIO::WorkerResult batchCopy(const QList<KIO::BatchCopyItem> &items, KIO::JobFlags flags) override;
 * };
 * \endcode
 *
 * and declares in its JSON metadata, next to "copyFromFile" and "copyToFile", which copies it
 * handles:
 * \list
 * \li "batchCopy": both ends are on this protocol
 * \li "batchCopyFromFile": the sources are local files and the destinations are on this protocol
 * \li "batchCopyToFile": the sources are on this protocol and the destinations are local files
 * \endlist
 *
 * A worker without the interface fails the command with KIO::ERR_UNSUPPORTED_ACTION.
 *
 * \since 6.32
 */
class KIOCORE_EXPORT BatchCopyWorker
{
public:
    virtual ~BatchCopyWorker();

    /*!
     * Copies every file of \a items.
     *
     * The worker reports the outcome of each item with WorkerBase::batchCopyItemFinished(), in any
     * order, and at most once per item:
     * \list
     * \li WorkerResult::pass() when the file was copied.
     * \li WorkerResult::fail() with KIO::ERR_FILE_ALREADY_EXIST when the destination exists. The
     *     batch only creates new files and leaves the existing one as it is. The job asks the user
     *     and copies that file on its own.
     * \li WorkerResult::fail() with any other error when only this file failed, for example
     *     KIO::ERR_CANNOT_OPEN_FOR_READING. The worker removes what it wrote of that file and goes
     *     on with the next one.
     * \endlist
     *
     * The copy gets BatchCopyItem::modificationTime where the protocol can, as copy() does with the
     * "modified" metadata. When it is invalid, the copy keeps the modification time of the source
     * where the protocol can.
     *
     * The worker reports the bytes copied so far for the whole batch with processedSize(), and
     * checks wasKilled() between files and, for a large file, between chunks.
     *
     * \a flags is empty: Overwrite and Resume are never set.
     *
     * Returns WorkerResult::pass() once every item has an outcome. Returns WorkerResult::fail() when
     * an error stops the remaining items, for example KIO::ERR_DISK_FULL. The worker then removes
     * what it wrote of the file it was copying, and the items without an outcome count as not
     * copied.
     */
    Q_REQUIRED_RESULT virtual WorkerResult batchCopy(const QList<BatchCopyItem> &items, JobFlags flags) = 0;

protected:
    BatchCopyWorker() = default;
    BatchCopyWorker(const BatchCopyWorker &) = delete;
    BatchCopyWorker &operator=(const BatchCopyWorker &) = delete;
};

} // namespace KIO

#endif
