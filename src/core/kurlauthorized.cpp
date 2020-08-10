/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1997 Matthias Kalle Dalheimer <kalle@kde.org>
    SPDX-FileCopyrightText: 1998, 1999, 2000 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlauthorized.h"
#include <kprotocolinfo.h>
#include <QUrl>

namespace KAuthorized
{

KCONFIGCORE_EXPORT extern bool authorizeUrlActionInternal(const QString &action, const QUrl &_baseURL, const QUrl &_destURL, const QString &baseClass, const QString &destClass);

KCONFIGCORE_EXPORT extern void allowUrlActionInternal(const QString &action, const QUrl &_baseURL, const QUrl &_destURL);

}

namespace KUrlAuthorized
{

bool authorizeUrlAction(const QString &action, const QUrl &baseURL, const QUrl &destURL)
{
    const QString baseClass = baseURL.isEmpty() ? QString() : KProtocolInfo::protocolClass(baseURL.scheme());
    const QString destClass = destURL.isEmpty() ? QString() : KProtocolInfo::protocolClass(destURL.scheme());
    return KAuthorized::authorizeUrlActionInternal(action, baseURL, destURL, baseClass, destClass);
}

void allowUrlAction(const QString &action, const QUrl &_baseURL, const QUrl &_destURL)
{
    if (authorizeUrlAction(action, _baseURL, _destURL)) {
        return;
    }

    KAuthorized::allowUrlActionInternal(action, _baseURL, _destURL);
}

} // namespace
