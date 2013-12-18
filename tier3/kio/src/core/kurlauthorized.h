/* This file is part of the KDE libraries
    Copyright (C) 1997 Matthias Kalle Dalheimer (kalle@kde.org)
    Copyright (c) 1998, 1999 Waldo Bastian <bastian@kde.org>

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

#ifndef KURLAUTHORIZED_H
#define KURLAUTHORIZED_H

#include <kio/kiocore_export.h>
#include <kauthorized.h>

class QUrl;
class QString;

/**
* Kiosk authorization framework.
*/
namespace KUrlAuthorized
{
  /**
   * Returns whether a certain URL related action is authorized.
   *
   * @param action The name of the action. Known actions are
   *  - list (may be listed (e.g. in file selection dialog)),
   *  - link (may be linked to),
   *  - open (may open) and
   *  - redirect (may be redirected to)
   * @param baseUrl The url where the action originates from
   * @param destUrl The object of the action
   * @return true when the action is authorized, false otherwise.
   *
   * @since 5.0
   */
  KIOCORE_EXPORT bool authorizeUrlAction(const QString& action, const QUrl& baseUrl, const QUrl& destUrl);

  /**
   * Allow a certain URL action. This can be useful if your application
   * needs to ensure access to an application specific directory that may
   * otherwise be subject to KIOSK restrictions.
   * @param action The name of the action.
   * @param baseUrl The url where the action originates from
   * @param _destUrl The object of the action
   *
   * @since 5.0
   */
  KIOCORE_EXPORT void allowUrlAction(const QString& action, const QUrl& baseUrl, const QUrl&  _destUrl);
}

#endif
