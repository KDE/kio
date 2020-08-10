/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Shaun Reich <shaun.reich@kdemail.net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef JOBURLCACHE_H
#define JOBURLCACHE_H

#include <QObject>
#include <QStringList>

class JobUrlCache : public QObject
{
    Q_OBJECT
public:
    static JobUrlCache &instance();

    void requestJobUrlsChanged();

Q_SIGNALS:
    void jobUrlsChanged(const QStringList&);

private Q_SLOTS:
    /**
      * Connected to kuiserver's signal...
      * @p urlList the dest url list
      */
    void slotJobUrlsChanged(const QStringList &urlList);

private:
    JobUrlCache();
    virtual ~JobUrlCache();

    QStringList m_destUrls;

    friend class JobUrlCacheSingleton;
};

#endif // JOBURLCACHE_H
