/*****************************************************************************
*  This file is part of the KDE libraries                                    *
*  Copyright (C) 2009 by Shaun Reich <shaun.reich@kdemail.net>               *
*                                                                            *
*  This library is free software; you can redistribute it and/or modify      *
*  it under the terms of the GNU Lesser General Public License as published  *
*  by the Free Software Foundation; either version 2 of the License or (at   *
*  your option) any later version.                                           *
*                                                                            *
*  This library is distributed in the hope that it will be useful,           *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
*  Library General Public License for more details.                          *
*                                                                            *
*  You should have received a copy of the GNU Lesser General Public License  *
*  along with this library; see the file COPYING.LIB.                        *
*  If not, see <http://www.gnu.org/licenses/>.                               *
*****************************************************************************/

#include "joburlcache_p.h"
#include "kuiserver_interface.h"



class JobUrlCacheSingleton
{
public:
    JobUrlCache instance;
};

Q_GLOBAL_STATIC(JobUrlCacheSingleton, s_jobUrlCache)

JobUrlCache& JobUrlCache::instance()
{
    return s_jobUrlCache()->instance;
}

JobUrlCache::JobUrlCache() : QObject(0)
{
    org::kde::kuiserver *interface = new
        org::kde::kuiserver("org.kde.kuiserver", "/JobViewServer", QDBusConnection::sessionBus(), this);

    //connect to receive updates about the job urls
    connect(interface, SIGNAL(jobUrlsChanged(QStringList)),
    this, SLOT(slotJobUrlsChanged(QStringList)));

    //force signal emission
    interface->emitJobUrlsChanged();
}

JobUrlCache::~JobUrlCache()
{
}

void JobUrlCache::slotJobUrlsChanged(QStringList urlList)
{
    m_destUrls = urlList;
    emit jobUrlsChanged(urlList);
}

void JobUrlCache::requestJobUrlsChanged()
{
    emit jobUrlsChanged(m_destUrls);
}

#include "moc_joburlcache_p.cpp"
