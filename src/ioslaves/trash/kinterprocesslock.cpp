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

#include <QtCore/QEventLoop>
#include <QtDBus/QDBusConnectionInterface>

#include <QtCore/QDebug>

class KInterProcessLockPrivate
{
    Q_DECLARE_PUBLIC(KInterProcessLock)
     KInterProcessLock *q_ptr;
    public:
        KInterProcessLockPrivate(const QString &resource, KInterProcessLock *parent)
            : m_resource(resource), m_parent(parent)
        {
            m_serviceName = QString::fromLatin1("org.kde.private.lock-%1").arg(m_resource);

            m_parent->connect(QDBusConnection::sessionBus().interface(), SIGNAL(serviceRegistered(const QString&)),
                              m_parent, SLOT(_k_serviceRegistered(const QString&)));
        }

        ~KInterProcessLockPrivate()
        {
        }

        void _k_serviceRegistered(const QString &service)
        {
            if (service == m_serviceName)
                emit m_parent->lockGranted(m_parent);
        }

        QString m_resource;
        QString m_serviceName;
        KInterProcessLock *m_parent;
};

KInterProcessLock::KInterProcessLock(const QString &resource)
    : d_ptr(new KInterProcessLockPrivate(resource, this))
{
    d_ptr->q_ptr = this;
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
    connect(this, SIGNAL(lockGranted(KInterProcessLock*)), &loop, SLOT(quit()));
    loop.exec();
}

#include "moc_kinterprocesslock.cpp"
