/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Shaun Reich <shaun.reich@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "joburlcache_p.h"
#include "kuiserver_interface.h"

class JobUrlCacheSingleton
{
public:
    JobUrlCache instance;
};

Q_GLOBAL_STATIC(JobUrlCacheSingleton, s_jobUrlCache)

JobUrlCache &JobUrlCache::instance()
{
    return s_jobUrlCache()->instance;
}

JobUrlCache::JobUrlCache() : QObject(nullptr)
{
    org::kde::kuiserver *interface = new
    org::kde::kuiserver(QStringLiteral("org.kde.kuiserver"), QStringLiteral("/JobViewServer"), QDBusConnection::sessionBus(), this);

    //connect to receive updates about the job urls
    connect(interface, &OrgKdeKuiserverInterface::jobUrlsChanged,
            this, &JobUrlCache::slotJobUrlsChanged);

    //force signal emission
    interface->emitJobUrlsChanged();
}

JobUrlCache::~JobUrlCache()
{
}

void JobUrlCache::slotJobUrlsChanged(const QStringList &urlList)
{
    m_destUrls = urlList;
    Q_EMIT jobUrlsChanged(urlList);
}

void JobUrlCache::requestJobUrlsChanged()
{
    Q_EMIT jobUrlsChanged(m_destUrls);
}

#include "moc_joburlcache_p.cpp"
