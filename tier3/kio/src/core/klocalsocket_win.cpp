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

// This file is a placeholder
// There is no Unix socket support on Windows

#include "klocalsocket_p.h"

#include "klocalizedstring.h"

void KLocalSocketPrivate::connectToPath(const QString &path, KLocalSocket::LocalSocketType aType,
                                        QAbstractSocket::OpenMode openMode)
{
    emitError(QAbstractSocket::UnsupportedSocketOperationError, i18n("Operation not supported"));
}

bool KLocalSocketServerPrivate::listen(const QString &path, KLocalSocket::LocalSocketType aType)
{
    emitError(QAbstractSocket::UnsupportedSocketOperationError, i18n("Operation not supported"));
    return false;
}

void KLocalSocketServerPrivate::close()
{
}

bool KLocalSocketServerPrivate::waitForNewConnection(int, bool *)
{
    Q_ASSERT_X(false, "KLocalSocketServer::waitForNewConnection",
               "This function should never have been called!");
    return false;
}

void KLocalSocketServerPrivate::_k_newConnectionActivity()
{
}
