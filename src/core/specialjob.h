/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_SPECIALJOB_H
#define KIO_SPECIALJOB_H

#include "transferjob.h"

namespace KIO
{
class SpecialJobPrivate;

/**
 * @class KIO::SpecialJob specialjob.h <KIO/SpecialJob>
 *
 * A class that sends a special command to a KIO worker.
 * This allows you to send a binary blob to a worker and handle
 * its responses. The worker will receive the binary data as an
 * argument to the "special" function (inherited from WorkerBase::special()).
 *
 * Use this only on KIO workers that belong to your application. Sending
 * special commands to other workers may cause unexpected behaviour.
 *
 * @see KIO::special
 */
class KIOCORE_EXPORT SpecialJob : public TransferJob
{
    Q_OBJECT
public:
    /**
     * Creates a KIO::SpecialJob.
     *
     * @param url the URL to be passed to the worker
     * @param data the data to be sent to the WorkerBase::special() function.
     */
    explicit SpecialJob(const QUrl &url, const QByteArray &data = QByteArray());

    /**
     * Sets the QByteArray that is passed to WorkerBase::special() on
     * the worker.
     */
    void setArguments(const QByteArray &data);

    /**
     * Returns the QByteArray data that will be sent (or has been sent) to the
     * worker.
     */
    QByteArray arguments() const;

public:
    ~SpecialJob() override;

private:
    Q_DECLARE_PRIVATE(SpecialJob)
};

}

#endif
