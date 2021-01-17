/*
    This file is part of the KDE
    SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-only
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

        q_ptr->connect(QDBusConnection::sessionBus().interface(), &QDBusConnectionInterface::serviceRegistered,
                       q_ptr, [this](const QString &service) { _k_serviceRegistered(service); });
    }

    ~KInterProcessLockPrivate()
    {
    }

    void _k_serviceRegistered(const QString &service)
    {
        if (service == m_serviceName) {
            Q_EMIT q_ptr->lockGranted(q_ptr);
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
