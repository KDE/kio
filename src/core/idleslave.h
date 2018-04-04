/*
   This file is part of the KDE libraries
   Copyright (c) 1999 Waldo Bastian <bastian@kde.org>
   Copyright (c) 2013 David Faure <faure@kde.org>

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

#ifndef IDLESLAVE_H
#define IDLESLAVE_H

#include "kiocore_export.h"
#include <QObject>
#include <QDateTime>
#include <QUrl>
#include <QScopedPointer>

namespace KIO
{

class IdleSlavePrivate;
class Connection;

/**
 * @internal
 * Represents an idle slave, waiting to be reused.
 * Used by klauncher.
 * Do not use outside KIO and klauncher!
 * @since 5.0
 */
class KIOCORE_EXPORT IdleSlave : public QObject
{
    Q_OBJECT
public:
    explicit IdleSlave(QObject *parent);
    ~IdleSlave();

    bool match(const QString &protocol, const QString &host, bool connected) const;
    void connect(const QString &app_socket);
    qint64 pid() const;
    int age(const QDateTime &now) const;
    void reparseConfiguration();
    bool onHold(const QUrl &url) const;
    QString protocol() const;
    Connection *connection() const;
    bool hasTempAuthorization() const;

Q_SIGNALS:
    void statusUpdate(IdleSlave *);

private Q_SLOTS:
    void gotInput();

private:
    QScopedPointer<IdleSlavePrivate> d;
};

} // namespace KIO

#endif // IDLESLAVE_H
