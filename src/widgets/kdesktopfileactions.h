/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000, 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KDESKTOPFILEACTIONS_H
#define KDESKTOPFILEACTIONS_H

#include <KServiceAction>
#include "kiowidgets_export.h"
#include <QList>
#include <QUrl>
#include <QDebug>
class KDesktopFile;
class KService;

/**
 * KDesktopFileActions provides a number of methods related to actions in desktop files.
 */
namespace KDesktopFileActions
{
/**
 * Returns a list of services for the given .desktop file that are handled
 * by kio itself. Namely mount/unmount for FSDevice files.
 * @return the list of services
 */
KIOWIDGETS_EXPORT QList<KServiceAction> builtinServices(const QUrl &url);

/**
 * Returns a list of services defined by the user as possible actions
 * on the given .desktop file. May include separators (see KServiceAction::isSeparator)
 * which should appear in user-visible representations of those actions,
 * such as separators in a menu.
 * @param path the path to the desktop file describing the services
 * @param bLocalFiles true if those services are to be applied to local files only
 * (if false, services that don't have %u or %U in the Exec line won't be taken into account).
 * @return the list of user defined actions
 */
KIOWIDGETS_EXPORT QList<KServiceAction> userDefinedServices(const QString &path, bool bLocalFiles);

/**
 * Overload of userDefinedServices but also allows you to pass a list of urls for this file.
 * This allows for the menu to be changed depending on the exact files via
 * the X-KDE-GetActionMenu extension.
 */
KIOWIDGETS_EXPORT QList<KServiceAction> userDefinedServices(const QString &path, const KDesktopFile &desktopFile, bool bLocalFiles, const QList<QUrl> &file_list = QList<QUrl>());

/**
 * Returns a list of services defined by the user as possible actions
 * on the given .desktop file represented by the KService instance.
 * May include separators (see KServiceAction::isSeparator) which should
 * appear in user-visible representations of those actions,
 * such as separators in a menu.
 * @param path the path to the desktop file describing the services
 * @param bLocalFiles true if those services are to be applied to local files only
 * (if false, services that don't have %u or %U in the Exec line won't be taken into account).
 * @param file_list list of urls; this allows for the menu to be changed depending on the exact files via
 * the X-KDE-GetActionMenu extension.
 *
 * @return the list of user defined actions
 */
KIOWIDGETS_EXPORT QList<KServiceAction> userDefinedServices(const KService &service, bool bLocalFiles, const QList<QUrl> &file_list = QList<QUrl>());

/**
  * Execute @p service on the list of @p urls.
  * @param urls the list of urls
  * @param service the service to execute
  */
KIOWIDGETS_EXPORT void executeService(const QList<QUrl> &urls, const KServiceAction &service);

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 71)
/**
 * Invokes the default action for the desktop entry. If the desktop
 * entry is not local, then only false is returned. Otherwise we
 * would create a security problem. Only types Link and Mimetype
 * could be followed.
 *
 * @param _url the url to run
 * @param _is_local true if the URL is local, false otherwise
 * @return true on success and false on failure.
 * @see KRun::runUrl
 * @deprecated since 5.71, use OpenUrlJob instead
 */
KIOWIDGETS_DEPRECATED_VERSION(5, 71, "Use KIO::OpenUrlJob instead")
KIOWIDGETS_EXPORT bool run(const QUrl &_url, bool _is_local);
#endif

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 71)
/**
 * Invokes the default action for the desktop entry. If the desktop
 * entry is not local, then only false is returned. Otherwise we
 * would create a security problem. Only types Link and Mimetype
 * could be followed.
 *
 * Use this function if a startup notification id has already been created.
 *
 * @param _url the url to run
 * @param _is_local true if the URL is local, false otherwise
 * @param asn Application startup notification id, if available
 * @return true on success and false on failure.
 * @see KRun::runUrl
 * @since 5.5
 * @deprecated since 5.71, use OpenUrlJob instead
 */
KIOWIDGETS_DEPRECATED_VERSION(5, 71, "Use KIO::OpenUrlJob instead")
KIOWIDGETS_EXPORT bool runWithStartup(const QUrl &_url, bool _is_local, const QByteArray &asn);
#endif
}

#endif
