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
#ifndef KLOCALSOCKETSERVERTEST_H
#define KLOCALSOCKETSERVERTEST_H

#include <QtCore/QObject>

class tst_KLocalSocketServer : public QObject
{
    Q_OBJECT
public:
    tst_KLocalSocketServer();
    ~tst_KLocalSocketServer();

private Q_SLOTS:
    void cleanup();

    void listen_data();
    void listen();

    void waitForConnection();
    void newConnection();

    void accept();

    void state();

    void setMaxPendingConnections();

    void abstractUnixSocket_data();
    void abstractUnixSocket();
};

#endif
