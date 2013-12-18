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

#include <kio/kiowidgets_export.h>
#include <ktcpsocket.h>

namespace KIO {
namespace SslUi {

enum RulesStorage {
    RecallRules = 1, ///< apply stored certificate rules (typically ignored errors)
    StoreRules = 2, ///< make new ignore rules from the user's choice and store them
    RecallAndStoreRules = 3 ///< apply stored rules and store new rules
};

bool KIOWIDGETS_EXPORT askIgnoreSslErrors(const KTcpSocket *socket,
                                   RulesStorage storedRules = RecallAndStoreRules);
bool KIOWIDGETS_EXPORT askIgnoreSslErrors(const KSslErrorUiData &uiData,
                                   RulesStorage storedRules = RecallAndStoreRules);
}
}

#endif
