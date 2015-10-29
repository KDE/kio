/* This file is part of the KDE libraries
   Copyright (C) 1999 Torben Weis <weis@kde.org>
   Copyright (C) 2003 Waldo Bastian <bastian@kde.org>
   Copyright     2012 David Faure <faure@kde.org>

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

#include "kprotocolinfofactory_p.h"
#include "kprotocolinfo_p.h"

#include <KPluginLoader>
#include <KPluginMetaData>

#include <QDirIterator>
#include <qstandardpaths.h>

Q_GLOBAL_STATIC(KProtocolInfoFactory, kProtocolInfoFactoryInstance)

KProtocolInfoFactory *KProtocolInfoFactory::self()
{
    return kProtocolInfoFactoryInstance();
}

KProtocolInfoFactory::KProtocolInfoFactory()
    : m_allProtocolsLoaded(false)
{
}

KProtocolInfoFactory::~KProtocolInfoFactory()
{
    QMutexLocker locker(&m_mutex);
    qDeleteAll(m_cache);
    m_cache.clear();
    m_allProtocolsLoaded = false;
}

QStringList KProtocolInfoFactory::protocols()
{
    QMutexLocker locker(&m_mutex);

    // fill cache, if not already done and use it
    fillCache();
    return m_cache.keys();
}

QList<KProtocolInfoPrivate *> KProtocolInfoFactory::allProtocols()
{
    QMutexLocker locker(&m_mutex);

    // fill cache, if not already done and use it
    fillCache();
    return m_cache.values();
}

KProtocolInfoPrivate *KProtocolInfoFactory::findProtocol(const QString &protocol)
{
    QMutexLocker locker(&m_mutex);

    // fill cache, if not already done and use it
    fillCache();
    return m_cache.value(protocol);
}

void KProtocolInfoFactory::fillCache()
{
    // mutex MUST be locked from the outside!
    Q_ASSERT(!m_mutex.tryLock());

    // no work if filled
    if (m_allProtocolsLoaded) {
        return;
    }

    // first: search for meta data protocol info, that might be bundled with applications
    // we search in all library paths inside kf5/kio
    Q_FOREACH (const KPluginMetaData &md, KPluginLoader::findPlugins("kf5/kio")) {
        // get slave name & protocols it supports, if any
        const QString slavePath = md.fileName();
        const QJsonObject protocols(md.rawData().value(QStringLiteral("KDE-KIO-Protocols")).toObject());

        // add all protocols, does nothing if object invalid
        for (auto it = protocols.begin(); it != protocols.end(); ++it) {
            // skip empty objects
            const QJsonObject protocol(it.value().toObject());
            if (protocol.isEmpty()) {
                continue;
            }

            // add to cache, skip double entries
            if (!m_cache.contains(it.key())) {
                m_cache.insert(it.key(), new KProtocolInfoPrivate(it.key(), slavePath, protocol));
            }
        }
    }

    // second: fallback to .protocol files
    Q_FOREACH (const QString &serviceDir, QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QLatin1String("kservices5"), QStandardPaths::LocateDirectory)) {
        QDirIterator it(serviceDir);
        while (it.hasNext()) {
            const QString file = it.next();
            if (file.endsWith(QLatin1String(".protocol"))) {
                const QString prot = it.fileInfo().baseName();
                // add to cache, skip double entries
                if (!m_cache.contains(prot)) {
                    m_cache.insert(prot, new KProtocolInfoPrivate(file));
                }
            }
        }
    }

    // all done, don't do it again
    m_allProtocolsLoaded = true;
}
