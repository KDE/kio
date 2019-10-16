/* This file is part of the KDE project
 *
 * Copyright (C) 2009 Andreas Hartmetz <ahartmetz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _KSSLUI_H
#define _KSSLUI_H

#include "kiowidgets_export.h"
#include <ksslerroruidata.h>
#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 64)
#include <ktcpsocket.h>
#endif

namespace KIO
{
/** UI methods for handling SSL errors. */
namespace SslUi
{

/** Error rule storage behavior. */
enum RulesStorage {
    RecallRules = 1, ///< apply stored certificate rules (typically ignored errors)
    StoreRules = 2, ///< make new ignore rules from the user's choice and store them
    RecallAndStoreRules = 3 ///< apply stored rules and store new rules
};

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 64)
/**
 * @deprecated since 5.64 use the KSslErrorUiData variant instead.
 */
KIOWIDGETS_DEPRECATED_VERSION(5, 64, "Use KIO::SslUi::askIgnoreSslErrors(const KSslErrorUiData &, RulesStorage)")
bool KIOWIDGETS_EXPORT askIgnoreSslErrors(const KTcpSocket *socket,
        RulesStorage storedRules = RecallAndStoreRules);
#endif

/**
 * If there are errors while establishing an SSL encrypted connection to a peer, usually due to
 * certificate issues, and since this poses a security issue, we need confirmation from the user about
 * how they wish to proceed.
 *
 * This function provides a dialog asking the user if they wish to abort the connection or ignore
 * the SSL errors that occurred and continue connecting. And in case of the latter whether to remember
 * the decision in the future or ignore the error temporarily.
 *
 * @p uiData the KSslErrorUiData object constructed from the socket that is trying to establish the
 *           encrypted connection
 * @p storedRules see RulesStorage Enum
 */
bool KIOWIDGETS_EXPORT askIgnoreSslErrors(const KSslErrorUiData &uiData,
        RulesStorage storedRules = RecallAndStoreRules);
}
}

#endif
