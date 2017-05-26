/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2013 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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
