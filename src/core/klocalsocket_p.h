/*
 * This file is part of the KDE libraries
 * Copyright (C) 2007 Thiago Macieira <thiago@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef KLOCALSOCKET_P_H
#define KLOCALSOCKET_P_H

#include <QtCore/QString>
#include <QtCore/QQueue>
#include "klocalsocket.h"
#define MIN_SOCKADDR_UN_LEN	(sizeof(quint16) + sizeof(char))

class QSocketNotifier;

class KLocalSocketPrivate
{
public:
    KLocalSocket * const q;
    KLocalSocketPrivate(KLocalSocket *qq)
        : q(qq), type(KLocalSocket::UnknownLocalSocketType)
        { }

    QString localPath;
    QString peerPath;
    KLocalSocket::LocalSocketType type;

    void connectToPath(const QString &path, KLocalSocket::LocalSocketType type,
                       QAbstractSocket::OpenMode openMode);

    void emitError(QAbstractSocket::SocketError, const QString &errorString);

    static inline KLocalSocketPrivate *d(KLocalSocket *aq)
        { return aq->d; }
};

class KLocalSocketServerPrivate
{
public:
    KLocalSocketServer * const q;
    KLocalSocketServerPrivate(KLocalSocketServer *qq);

    int descriptor;
    int maxPendingConnections;
    QAbstractSocket::SocketState state;
    QAbstractSocket::SocketError error;
    KLocalSocket::LocalSocketType type;
    QString localPath;
    QString errorString;

    QSocketNotifier *readNotifier;
    QQueue<KLocalSocket *> pendingConnections;

    bool listen(const QString &path, KLocalSocket::LocalSocketType type);
    void close();
    bool waitForNewConnection(int msec, bool *timedOut);
    bool processSocketActivity();
    void _k_newConnectionActivity();
    void emitError(QAbstractSocket::SocketError, const QString &errorString);
};

#endif
