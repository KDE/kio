/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_BATCHCOPYJOB_P_H
#define KIO_BATCHCOPYJOB_P_H

#include "batchcopyworker.h"
#include "simplejob.h"

namespace KIO
{
class BatchCopyJobPrivate;

// Sends one CMD_BATCH_COPY to the worker of its url.
class KIOCORE_EXPORT BatchCopyJob : public SimpleJob
{
    Q_OBJECT

public:
    ~BatchCopyJob() override;

Q_SIGNALS:
    // The outcome the worker reported for the item at index of the request. error is 0 when the item was
    // copied, KIO::ERR_FILE_ALREADY_EXIST when its destination exists.
    void itemFinished(int index, int error, const QString &errorText);

protected:
    KIOCORE_NO_EXPORT explicit BatchCopyJob(BatchCopyJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(BatchCopyJob)
};

// workerUrl picks the worker: the destination of a copy from local files, the source of a copy to
// local files.
KIOCORE_EXPORT BatchCopyJob *batchCopy(const QUrl &workerUrl, const QList<BatchCopyItem> &items, JobFlags flags = DefaultFlags);
}

#endif
