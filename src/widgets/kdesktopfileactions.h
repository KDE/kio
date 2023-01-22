/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000, 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KDESKTOPFILEACTIONS_H
#define KDESKTOPFILEACTIONS_H

#include "kiowidgets_export.h"
#include <KServiceAction>
#include <QDebug>
#include <QList>
#include <QUrl>
class KDesktopFile;
class KService;

/**
 * KDesktopFileActions provides a number of methods related to actions in desktop files.
 */
namespace KDesktopFileActions
{
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
}

#endif
