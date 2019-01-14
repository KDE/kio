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

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <winsock2.h> // struct timeval
#endif

#include "global.h"
#include "connection_p.h"
#include <QTimer>
#include <QPointer>
#include <QHostInfo>

#include "kiocoredebug.h"

static const unsigned int max_nums = 8;

class KIO::SlaveInterfacePrivate
{
public:
    SlaveInterfacePrivate()
        : connection(nullptr), filesize(0), offset(0), last_time(0), start_time(0),
          nums(0), slave_calcs_speed(false)
    {
    }
    virtual ~SlaveInterfacePrivate()
    {
        delete connection;
    }

    Connection *connection;
    QTimer speed_timer;

    // We need some metadata here for our SSL code in messageBox() and for sslMetaData().
    MetaData sslMetaData;

    KIO::filesize_t sizes[max_nums];
    qint64 times[max_nums];

    KIO::filesize_t filesize, offset;
    size_t last_time;
    qint64 start_time;
    uint nums;
    bool slave_calcs_speed;

    void slotHostInfo(const QHostInfo &info);
};

#endif
