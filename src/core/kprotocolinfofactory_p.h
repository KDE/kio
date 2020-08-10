/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000, 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#ifndef kprotocolinfofactory_h
#define kprotocolinfofactory_h

#include <QHash>
#include <QString>
#include <QStringList>
#include <QMutex>

class KProtocolInfoPrivate;

/**
 * @internal
 *
 * KProtocolInfoFactory is a factory for getting
 * KProtocolInfo. The factory is a singleton
 * (only one instance can exist).
 */
class KProtocolInfoFactory
{
public:
    /**
     * @return the instance of KProtocolInfoFactory (singleton).
     */
    static KProtocolInfoFactory *self();

    KProtocolInfoFactory();
    ~KProtocolInfoFactory();

    /**
     * Returns protocol info for @p protocol.
     *
     * Does not take proxy settings into account.
     * @param protocol the protocol to search for
     * @return the pointer to the KProtocolInfo, or @c nullptr if not found
     */
    KProtocolInfoPrivate *findProtocol(const QString &protocol);

    /**
     * Loads all protocols. Slow, obviously, but fills the cache once and for all.
     */
    QList<KProtocolInfoPrivate *> allProtocols();

    /**
     * Returns list of all known protocols.
     * @return a list of all protocols
     */
    QStringList protocols();

private:
    /**
     * Fill the internal cache.
     */
    bool fillCache();

    typedef QHash<QString, KProtocolInfoPrivate *> ProtocolCache;
    ProtocolCache m_cache;
    bool m_cacheDirty;
    mutable QMutex m_mutex; // protects m_cache and m_allProtocolsLoaded
};

#endif
