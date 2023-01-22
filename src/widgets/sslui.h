/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2009 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KSSLUI_H
#define _KSSLUI_H

#include "kiowidgets_export.h"
#include <ksslerroruidata.h>

namespace KIO
{
/** UI methods for handling SSL errors. */
namespace SslUi
{
/** Error rule storage behavior. */
enum RulesStorage {
    RecallRules = 1, ///< apply stored certificate rules (typically ignored errors)
    StoreRules = 2, ///< make new ignore rules from the user's choice and store them
    RecallAndStoreRules = 3, ///< apply stored rules and store new rules
};

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
bool KIOWIDGETS_EXPORT askIgnoreSslErrors(const KSslErrorUiData &uiData, RulesStorage storedRules = RecallAndStoreRules);
}
}

#endif
