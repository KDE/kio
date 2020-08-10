/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KPASSWDSERVERCLIENT_H
#define KPASSWDSERVERCLIENT_H

#include <qglobal.h>
#include <kiocore_export.h>

class QString;
class OrgKdeKPasswdServerInterface;

namespace KIO
{
    class AuthInfo;
}

class KPasswdServerClientPrivate;

/**
 * @class KPasswdServerClient kpasswdserverclient.h <KPasswdServerClient>
 *
 * Interface class for kpasswdserver.
 * KIOSlaves should not use this directly but via the SlaveBase API.
 * @since 5.30
 */
class KIOCORE_EXPORT KPasswdServerClient
{
public:
    /**
     * Creates a client instance for kpasswdserver.
     * The instance should be kept for the lifetime of the process, not created for each request.
     */
    KPasswdServerClient();
    /**
     * Destructor.
     */
    ~KPasswdServerClient();

    KPasswdServerClient(const KPasswdServerClient &) = delete;
    KPasswdServerClient& operator=(const KPasswdServerClient &) = delete;

    /**
     * Check if kpasswdserver has cached authentication information regarding
     * an AuthInfo object.
     * @param info information to check cache for
     * @param windowId used as parent for dialogs, comes from QWidget::winId() on the toplevel widget
     * @param usertime the X11 user time from the calling application, so that any dialog
     *                 (e.g. wallet password) respects focus-prevention rules.
     *                 Use KUserTimestamp::userTimestamp in the GUI application from which the request originates.
     * @return true if kpasswdserver provided cached information, false if not
     * @remarks info will contain the results of the check. To see if
     *          information was retrieved, check info.isModified().
     */
    bool checkAuthInfo(KIO::AuthInfo *info, qlonglong windowId,
                       qlonglong usertime);

    /**
     * Let kpasswdserver ask the user for authentication information.
     * @param info information to query the user for
     * @param errorMsg error message that will be displayed to the user
     * @param windowId used as parent for dialogs, comes from QWidget::winId() on the toplevel widget
     * @param usertime the X11 user time from the calling application, so that the dialog
     *                 (e.g. wallet password) respects focus-prevention rules.
     *                 Use KUserTimestamp::userTimestamp in the GUI application from which the request originates.
     * @return a KIO error code: KJob::NoError (0) on success, otherwise ERR_USER_CANCELED if the user canceled,
     *  or ERR_PASSWD_SERVER if we couldn't communicate with kpasswdserver.
     * @remarks If NoError is returned, then @p info will contain the authentication information that was retrieved.
     */
    int queryAuthInfo(KIO::AuthInfo *info, const QString &errorMsg,
                      qlonglong windowId, qlonglong usertime);

    /**
     * Manually add authentication information to kpasswdserver's cache.
     * @param info information to add
     * @param windowId used as parent window for dialogs, comes from QWidget::winId() on the toplevel widget
     */
    void addAuthInfo(const KIO::AuthInfo &info, qlonglong windowId);

    /**
     * Manually remove authentication information from kpasswdserver's cache.
     * @param host hostname of the information to remove
     * @param protocol protocol to remove information for
     * @param user username to remove information for
     */
    void removeAuthInfo(const QString &host, const QString &protocol,
                        const QString &user);

private:
    OrgKdeKPasswdServerInterface *m_interface;
    KPasswdServerClientPrivate *d;
};

#endif
