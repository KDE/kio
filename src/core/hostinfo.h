/*
    SPDX-FileCopyrightText: 2008 Roland Harnau <tau@gmx.eu>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef HOSTINFO_H_
#define HOSTINFO_H_

#include "kiocore_export.h"
#include <QObject>
#include <QString>

class QHostInfo;

namespace KIO
{
/**
 * @internal
 * WARNING: this could disappear at some point in time.
 * DO NOT USE outside KDE Frameworks
 */
/*
 * TODO KF6: This header is intenionally not installed in KF6, and we should look into unexporting
 * and removing most of these functions; they are only used internally in very few places where
 * they might even be inlined to.
 */
namespace HostInfo
{
/// @internal
void lookupHost(const QString &hostName, QObject *receiver, const char *member);
/// @internal
KIOCORE_EXPORT QHostInfo lookupHost(const QString &hostName, unsigned long timeout);
/// @internal
KIOCORE_EXPORT QHostInfo lookupCachedHostInfoFor(const QString &hostName);
/// @internal
KIOCORE_EXPORT void cacheLookup(const QHostInfo &info);
}
}

#endif
