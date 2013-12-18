/* This file is part of the KDE libraries
    Copyright (C) 1997 Matthias Kalle Dalheimer (kalle@kde.org)
    Copyright (C) 1998, 1999, 2000 Waldo Bastian <bastian@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kurlauthorized.h"
#include <kprotocolinfo.h>
#include <QUrl>

namespace KAuthorized {

KCONFIGCORE_EXPORT extern bool authorizeUrlActionInternal(const QString& action, const QUrl &_baseURL, const QUrl &_destURL, const QString& baseClass, const QString& destClass);

KCONFIGCORE_EXPORT extern void allowUrlActionInternal(const QString &action, const QUrl &_baseURL, const QUrl &_destURL);

}

namespace KUrlAuthorized {

bool authorizeUrlAction(const QString &action, const QUrl &baseURL, const QUrl &destURL)
{
    QString baseClass = KProtocolInfo::protocolClass(baseURL.scheme());
    QString destClass = KProtocolInfo::protocolClass(destURL.scheme());
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
