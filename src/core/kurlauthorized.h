/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1997 Matthias Kalle Dalheimer <kalle@kde.org>
    SPDX-FileCopyrightText: 1998, 1999 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLAUTHORIZED_H
#define KURLAUTHORIZED_H

#include "kiocore_export.h"
#include <KAuthorized>

class QUrl;
class QString;

/**
 * The functions in this namespace allow actions to be restricted based
 * on the URL they operate on (see the KAuthorized namespace in
 * KConfig).
 *
 * As with KAuthorized functions, the relevant settings are read from
 * the application's KSharedConfig instance, so actions can be disabled
 * on a per-application or global basis (by using the kdeglobals file).
 *
 * URLs can be matched based on protocol, host and path, and the
 * referring URL can be taken into account.
 *
 * URL-based restrictions are recorded using this syntax:
 * @verbatim
   [KDE URL Restrictions]
   rule_count=<N>
   rule_1=<action>,<referingURL_protocol>,<referingURL_host>,<referingURL_path>,<URL_protocol>,<URL_host>,<URL_path>,<enabled>
   ...
   rule_N=<action>,<referingURL_protocol>,<referingURL_host>,<referingURL_path>,<URL_protocol>,<URL_host>,<URL_path>,<enabled>
   @endverbatim
 *
 * The following standard actions are defined:
 *
 * - redirect: A common example is a web page redirecting to another web
 *             page.  By default, internet protocols are not permitted
 *             to redirect to the "file" protocol, but you could
 *             override this for a specific host, for example:
 *             @verbatim
               [KDE URL Restrictions]
               rule_count=1
               rule_1=redirect,http,myhost.example.com,,file,,,true
               @endverbatim
 * - list:     Determines whether a URL can be browsed, in an "open" or
 *             "save" dialog, for example.  If a user should only be
 *             able to browse files under home directory one could use:
 *             @verbatim
               [KDE URL Restrictions]
               rule_count=2
               rule_1=list,,,,file,,,false
               rule_2=list,,,,file,,$HOME,true
               @endverbatim
 *             The first rule disables browsing any directories on the
 *             local filesystem. The second rule then enables browsing
 *             the users home directory.
 * - open:     This controls which files can be opened by the user in
 *             applications. It also affects where users can save files.
 *             To only allow a user to open the files in his own home
 *             directory one could use:
 *             @verbatim
               [KDE URL Restrictions]
               rule_count=3
                 rule_1=open,,,,file,,,false
                 rule_2=open,,,,file,,$HOME,true
                 rule_3=open,,,,file,,$TMP,true
               @endverbatim
 *             Note that with the above, users would still be able to
 *             open files from the internet. Note also that the user is
 *             also given access to $TMP in order to ensure correct
 *             operation of KDE applications. $TMP is replaced with the
 *             temporary directory that KDE uses for this user.
 * - link:     Determines whether a URL can be linked to.
 *
 * Some remarks:
 * - empty entries match everything
 * - host names may start with a wildcard, e.g. "*.acme.com"
 * - a protocol also matches similar protocols that start with the same name,
 *   e.g. "http" matches both http and https. You can use "http!" if you only want to
 *   match http (and not https)
 * - specifying a path matches all URLs that start with the same path. For better results
 *   you should not include a trailing slash. If you want to specify one specific path, you can
 *   add an exclamation mark. E.g. "/srv" matches both "/srv" and "/srv/www" but "/srv!" only
 *   matches "/srv" and not "/srv/www".
*/
namespace KUrlAuthorized
{
/**
 * Returns whether a certain URL related action is authorized.
 *
 * @param action   The name of the action, typically one of "list",
 *                 "link", "open" or "redirect".
 * @param baseUrl  The url where the action originates from.
 * @param destUrl  The object of the action.
 * @return         @c true if the action is authorized, @c false
 *                 otherwise.
 *
 * @see allowUrlAction()
 * @since 5.0
 */
KIOCORE_EXPORT bool authorizeUrlAction(const QString &action, const QUrl &baseUrl, const QUrl &destUrl);

/**
 * Override Kiosk restrictions to allow a given URL action.
 *
 * This can be useful if your application needs to ensure access to an
 * application-specific directory that may otherwise be subject to Kiosk
 * restrictions.
 *
 * @param action   The name of the action.
 * @param baseUrl  The url where the action originates from.
 * @param destUrl  The object of the action.
 *
 * @see authorizeUrlAction()
 * @since 5.0
 */
KIOCORE_EXPORT void allowUrlAction(const QString &action, const QUrl &baseUrl, const QUrl &destUrl);

}

#endif
