/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_SLAVEINTERFACEPRIVATE_H
#define KIO_SLAVEINTERFACEPRIVATE_H

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <winsock2.h> // struct timeval
#endif

#include "connection_p.h"
#include "global.h"
#include <QHostInfo>
#include <QTimer>

#include "kiocoredebug.h"

static const unsigned int max_nums = 8;

class KIO::SlaveInterfacePrivate
{
public:
    SlaveInterfacePrivate()
        : connection(nullptr)
        , filesize(0)
        , offset(0)
        , last_time(0)
        , start_time(0)
        , nums(0)
        , slave_calcs_speed(false)
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

    // Used to cache privilege operation details passed from the worker by the metadata hack
    // See WORKER_MESSAGEBOX_DETAILS_HACK
    QString messageBoxDetails;

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
