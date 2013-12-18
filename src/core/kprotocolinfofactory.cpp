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
#include <QDirIterator>
#include <qstandardpaths.h>

Q_GLOBAL_STATIC(KProtocolInfoFactory, kProtocolInfoFactoryInstance)

KProtocolInfoFactory* KProtocolInfoFactory::self()
{
    return kProtocolInfoFactoryInstance();
}

KProtocolInfoFactory::KProtocolInfoFactory()
    : m_allProtocolsLoaded(false)
{
}

KProtocolInfoFactory::~KProtocolInfoFactory()
{
    qDeleteAll(m_cache);
    m_cache.clear();
}

static QStringList servicesDirs() {
    return QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                     QLatin1String("kde5/services"),
                                     QStandardPaths::LocateDirectory);
}

QStringList KProtocolInfoFactory::protocols() const
{
    if (m_allProtocolsLoaded)
        return m_cache.keys();

    QStringList result;
    Q_FOREACH(const QString& serviceDir, servicesDirs()) {
        QDirIterator it(serviceDir);
        while (it.hasNext()) {
            const QString file = it.next();
            if (file.endsWith(QLatin1String(".protocol"))) {
                result.append(it.fileInfo().baseName());
            }
        }
    }
    return result;
}


QList<KProtocolInfoPrivate *> KProtocolInfoFactory::allProtocols()
{
    if (m_allProtocolsLoaded)
        return m_cache.values();

    QStringList result;
    Q_FOREACH(const QString& serviceDir, servicesDirs()) {
        QDirIterator it(serviceDir);
        while (it.hasNext()) {
            const QString file = it.next();
            if (file.endsWith(QLatin1String(".protocol"))) {
                const QString prot = it.fileInfo().baseName();
                m_cache.insert(prot, new KProtocolInfoPrivate(file));
            }
        }
    }
    m_allProtocolsLoaded = true;
    return m_cache.values();
}

KProtocolInfoPrivate* KProtocolInfoFactory::findProtocol(const QString &protocol)
{
    ProtocolCache::const_iterator it = m_cache.constFind(protocol);
    if (it != m_cache.constEnd())
        return *it;

    const QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kde5/services/") + protocol + QLatin1String(".protocol"));
    if (file.isEmpty())
        return 0;

    KProtocolInfoPrivate* info = new KProtocolInfoPrivate(file);
    m_cache.insert(protocol, info);
    return info;
}
