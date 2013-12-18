/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KIO_SLAVEINTERFACEPRIVATE_H
#define KIO_SLAVEINTERFACEPRIVATE_H

#include "global.h"
#include "connection_p.h"
#include <QtCore/QTimer>
#include <QtCore/QPointer>
#include <QtNetwork/QHostInfo>

static const unsigned int max_nums = 8;


class KIO::SlaveInterfacePrivate
{
public:
    SlaveInterfacePrivate()
        : connection(0), filesize(0), offset(0), last_time(0),
          nums(0), slave_calcs_speed(false)
    {
        start_time.tv_sec = 0;
        start_time.tv_usec = 0;
    }
    ~SlaveInterfacePrivate()
    {
        delete connection;
    }

    Connection *connection;
    QTimer speed_timer;

    // We need some metadata here for our SSL code in messageBox() and for sslMetaData().
    MetaData sslMetaData;

    KIO::filesize_t sizes[max_nums];
    long times[max_nums];

    KIO::filesize_t filesize, offset;
    size_t last_time;
    struct timeval start_time;
    uint nums;
    bool slave_calcs_speed;

    void slotHostInfo(const QHostInfo& info);
};

#endif
