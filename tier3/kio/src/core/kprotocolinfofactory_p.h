/* This file is part of the KDE libraries
   Copyright (C) 1999 Torben Weis <weis@kde.org>
   Copyright (C) 2000,2003 Waldo Bastian <bastian@kde.org>
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
#ifndef kprotocolinfofactory_h
#define kprotocolinfofactory_h

#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QStringList>

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
    static KProtocolInfoFactory* self();

    KProtocolInfoFactory();
    ~KProtocolInfoFactory();

    /*
     * Returns protocol info for @p protocol.
     *
     * Does not take proxy settings into account.
     * @param protocol the protocol to search for
     * @return the pointer to the KProtocolInfo, or 0 if not found
     */
    KProtocolInfoPrivate* findProtocol(const QString &protocol);

    /**
     * Loads all protocols. Slow, obviously, but fills the cache once and for all.
     */
    QList<KProtocolInfoPrivate *> allProtocols();

    /**
     * Returns list of all known protocols.
     * @return a list of all protocols
     */
    QStringList protocols() const;

private:
    typedef QHash<QString, KProtocolInfoPrivate *> ProtocolCache;
    ProtocolCache m_cache;
    bool m_allProtocolsLoaded;
};

#endif
