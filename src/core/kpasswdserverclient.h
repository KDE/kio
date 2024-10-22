/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KPASSWDSERVERCLIENT_H
#define KPASSWDSERVERCLIENT_H

#include <kiocore_export.h>
#include <qglobal.h>

#include <memory>

class QString;
class OrgKdeKPasswdServerInterface;

namespace KIO
{
class AuthInfo;
}

class KPasswdServerClientPrivate;

/*!
 * \class KPasswdServerClient
 * \inmodule KIOCore
 *
 * \brief Interface class for kpasswdserver.
 *
 * KIO workers should not use this directly but via the WorkerBase API.
 * \since 5.30
 */
class KIOCORE_EXPORT KPasswdServerClient
{
public:
    /*!
     * Creates a client instance for kpasswdserver.
     * The instance should be kept for the lifetime of the process, not created for each request.
     */
    KPasswdServerClient();
    ~KPasswdServerClient();

    KPasswdServerClient(const KPasswdServerClient &) = delete;
    KPasswdServerClient &operator=(const KPasswdServerClient &) = delete;

    /*!
     * Check if kpasswdserver has cached authentication information regarding
     * an AuthInfo object.
     *
     * \a info information to check cache for
     *
     * \a windowId used as parent for dialogs, comes from QWidget::winId() on the toplevel widget
     *
     * \a usertime the X11 user time from the calling application, so that any dialog
     *                 (e.g. wallet password) respects focus-prevention rules.
     *                 Use KUserTimestamp::userTimestamp in the GUI application from which the request originates.
     *
     * Returns \c true if kpasswdserver provided cached information, false if not
     *
     * Info will contain the results of the check. To see if
     *          information was retrieved, check info.isModified().
     */
    bool checkAuthInfo(KIO::AuthInfo *info, qlonglong windowId, qlonglong usertime);

    /*!
     * Let kpasswdserver ask the user for authentication information.
     *
     * \a info information to query the user for
     *
     * \a errorMsg error message that will be displayed to the user
     *
     * \a windowId used as parent for dialogs, comes from QWidget::winId() on the toplevel widget
     *
     * \a usertime the X11 user time from the calling application, so that the dialog
     *                 (e.g. wallet password) respects focus-prevention rules.
     *                 Use KUserTimestamp::userTimestamp in the GUI application from which the request originates.
     *
     * Returns a KIO error code: KJob::NoError (0) on success, otherwise ERR_USER_CANCELED if the user canceled,
     *  or ERR_PASSWD_SERVER if we couldn't communicate with kpasswdserver.
     *
     * If NoError is returned, then \a info will contain the authentication information that was retrieved.
     */
    int queryAuthInfo(KIO::AuthInfo *info, const QString &errorMsg, qlonglong windowId, qlonglong usertime);

    /*!
     * Manually add authentication information to kpasswdserver's cache.
     *
     * \a info information to add
     *
     * \a windowId used as parent window for dialogs, comes from QWidget::winId() on the toplevel widget
     */
    void addAuthInfo(const KIO::AuthInfo &info, qlonglong windowId);

    /*!
     * Manually remove authentication information from kpasswdserver's cache.
     *
     * \a host hostname of the information to remove
     *
     * \a protocol protocol to remove information for
     *
     * \a user username to remove information for
     */
    void removeAuthInfo(const QString &host, const QString &protocol, const QString &user);

private:
    OrgKdeKPasswdServerInterface *m_interface;
    std::unique_ptr<KPasswdServerClientPrivate> d;
};

#endif
