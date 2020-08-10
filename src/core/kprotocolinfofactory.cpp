/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kprotocolinfofactory_p.h"
#include "kprotocolinfo_p.h"

#include <KPluginLoader>
#include <KPluginMetaData>

#include <QCoreApplication>
#include <QDirIterator>
#include <QStandardPaths>

#include "kiocoredebug.h"

Q_GLOBAL_STATIC(KProtocolInfoFactory, kProtocolInfoFactoryInstance)

KProtocolInfoFactory *KProtocolInfoFactory::self()
{
    return kProtocolInfoFactoryInstance();
}

KProtocolInfoFactory::KProtocolInfoFactory()
    : m_cacheDirty(true)
{
}

KProtocolInfoFactory::~KProtocolInfoFactory()
{
    QMutexLocker locker(&m_mutex);
    qDeleteAll(m_cache);
    m_cache.clear();
    m_cacheDirty = true;
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
    Q_ASSERT(!protocol.isEmpty());
    Q_ASSERT(!protocol.contains(QLatin1Char(':')));

    QMutexLocker locker(&m_mutex);

    const bool filled = fillCache();

    KProtocolInfoPrivate *info = m_cache.value(protocol);
    if (!info && !filled) {
        // Unknown protocol! Maybe it just got installed and our cache is out of date?
        qCDebug(KIO_CORE) << "Refilling KProtocolInfoFactory cache in the hope to find" << protocol;
        m_cacheDirty = true;
        fillCache();
        info = m_cache.value(protocol);
    }
    return info;
}

bool KProtocolInfoFactory::fillCache()
{
    // mutex MUST be locked from the outside!
    Q_ASSERT(!m_mutex.tryLock());

    // no work if filled
    if (!m_cacheDirty) {
        return false;
    }

    qDeleteAll(m_cache);
    m_cache.clear();

    // first: search for meta data protocol info, that might be bundled with applications
    // we search in all library paths inside kf5/kio
    const QVector<KPluginMetaData> plugins = KPluginLoader::findPlugins(QStringLiteral("kf5/kio"));
    for (const KPluginMetaData &md : plugins) {
        // get slave name & protocols it supports, if any
        const QString slavePath = md.fileName();
        const QJsonObject protocols(md.rawData().value(QStringLiteral("KDE-KIO-Protocols")).toObject());
        qCDebug(KIO_CORE) << slavePath << "supports protocols" << protocols.keys();

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
    const QStringList serviceDirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("kservices5"), QStandardPaths::LocateDirectory)
        << QCoreApplication::applicationDirPath() + QLatin1String("/kservices5");
    for (const QString &serviceDir : serviceDirs) {
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
    m_cacheDirty = false;
    return true;
}
