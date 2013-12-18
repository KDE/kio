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

#ifndef JOBURLCACHE_H
#define JOBURLCACHE_H

#include <QObject>
#include <QStringList>

class JobUrlCache : public QObject
{
    Q_OBJECT
public:
    static JobUrlCache& instance();

    void requestJobUrlsChanged();

Q_SIGNALS:
    void jobUrlsChanged(QStringList);

private Q_SLOTS:
    /**
      * Connected to kuiserver's signal...
      * @p urlList the dest url list
      */
    void slotJobUrlsChanged(QStringList urlList);

private:
    JobUrlCache();
    virtual ~JobUrlCache();

    QStringList m_destUrls;

    friend class JobUrlCacheSingleton;
};

#endif // JOBURLCACHE_H
