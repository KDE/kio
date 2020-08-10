/*
    SPDX-FileCopyrightText: 2008 Roland Harnau <tau@gmx.eu>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef HOSTINFO_H_
#define HOSTINFO_H_

#include <QString>
#include <QObject>
#include "kiocore_export.h"

class QHostInfo;

namespace KIO
{
/**
 * @internal
 * WARNING: this could disappear at some point in time.
 * DO NOT USE outside KDE Frameworks
 */
namespace HostInfo
{
/// @internal
KIOCORE_EXPORT void lookupHost(const QString &hostName, QObject *receiver, const char *member);
/// @internal
KIOCORE_EXPORT QHostInfo lookupHost(const QString &hostName, unsigned long timeout);
/// @internal
KIOCORE_EXPORT QHostInfo lookupCachedHostInfoFor(const QString &hostName);
/// @internal
KIOCORE_EXPORT void cacheLookup(const QHostInfo &info);

// used by khtml's DNS prefetching feature
/// @internal
KIOCORE_EXPORT void prefetchHost(const QString &hostName);
/// @internal
KIOCORE_EXPORT void setCacheSize(int s);
/// @internal
KIOCORE_EXPORT void setTTL(int ttl);
}
}

#endif
