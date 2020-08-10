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
 * A class that sends a special command to an ioslave.
 * This allows you to send a binary blob to an ioslave and handle
 * its responses. The ioslave will receive the binary data as an
 * argument to the "special" function (inherited from SlaveBase::special()).
 *
 * Use this only on ioslaves that belong to your application. Sending
 * special commands to other ioslaves may cause unexpected behaviour.
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
     * @param url the URL to be passed to the ioslave
     * @param data the data to be sent to the SlaveBase::special() function.
     */
    explicit SpecialJob(const QUrl &url, const QByteArray &data = QByteArray());

    /**
     * Sets the QByteArray that is passed to SlaveBase::special() on
     * the ioslave.
     */
    void setArguments(const QByteArray &data);

    /**
     * Returns the QByteArray data that will be sent (or has been sent) to the
     * ioslave.
     */
    QByteArray arguments() const;

public:
    ~SpecialJob();

private:
    Q_DECLARE_PRIVATE(SpecialJob)
};

}

#endif
