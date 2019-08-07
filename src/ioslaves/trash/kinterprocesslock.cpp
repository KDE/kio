/*
    This file is part of the KDE

    Copyright (C) 2009 Tobias Koenig (tokoe@kde.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this library; see the file COPYING. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kinterprocesslock.h"
#include "kiotrashdebug.h"

#include <QEventLoop>
#include <QDBusConnectionInterface>


class KInterProcessLockPrivate
{
    Q_DECLARE_PUBLIC(KInterProcessLock)
    KInterProcessLock * const q_ptr;
public:
    KInterProcessLockPrivate(const QString &resource, KInterProcessLock *q)
        : q_ptr(q)
        , m_resource(resource)
    {
        m_serviceName = QStringLiteral("org.kde.private.lock-%1").arg(m_resource);

        q_ptr->connect(QDBusConnection::sessionBus().interface(), SIGNAL(serviceRegistered(QString)),
                       q_ptr, SLOT(_k_serviceRegistered(QString)));
    }

    ~KInterProcessLockPrivate()
    {
    }

    void _k_serviceRegistered(const QString &service)
    {
        if (service == m_serviceName) {
            emit q_ptr->lockGranted(q_ptr);
        }
    }

    QString m_resource;
    QString m_serviceName;
};

KInterProcessLock::KInterProcessLock(const QString &resource)
    : d_ptr(new KInterProcessLockPrivate(resource, this))
{
}

KInterProcessLock::~KInterProcessLock()
{
    delete d_ptr;
}

QString KInterProcessLock::resource() const
{
    return d_ptr->m_resource;
}

void KInterProcessLock::lock()
{
    QDBusConnection::sessionBus().interface()->registerService(d_ptr->m_serviceName,
            QDBusConnectionInterface::QueueService,
            QDBusConnectionInterface::DontAllowReplacement);
}

void KInterProcessLock::unlock()
{
    QDBusConnection::sessionBus().interface()->unregisterService(d_ptr->m_serviceName);
}

void KInterProcessLock::waitForLockGranted()
{
    QEventLoop loop;
    connect(this, &KInterProcessLock::lockGranted, &loop, &QEventLoop::quit);
    loop.exec();
}

#include "moc_kinterprocesslock.cpp"
