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
#ifndef KLOCALSOCKETTEST_H
#define KLOCALSOCKETTEST_H

#include <QtCore/QObject>

class KLocalSocketServer;
class tst_KLocalSocket : public QObject
{
    Q_OBJECT
public:
    KLocalSocketServer *server;
    tst_KLocalSocket();
    ~tst_KLocalSocket();

private Q_SLOTS:
    void initTestCase();
    void connection_data();
    void connection();
    void waitFor();

    void reading();
    void writing();

    void state();

    void connected();
};

#endif
