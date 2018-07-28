/* This file is part of the KDE project
   Copyright (C) 1998-2009 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2 of the License or
   ( at your option ) version 3 or, at the discretion of KDE e.V.
   ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KFILEITEMACTIONS_H
#define KFILEITEMACTIONS_H

#include <kservice.h>
#include <kfileitem.h>
#include "kiowidgets_export.h"

class KFileItemListProperties;
class QAction;
class QMenu;
class KFileItemActionsPrivate;

/**
 * @class KFileItemActions kfileitemactions.h <KFileItemActions>
 *
 * This class creates and handles the actions for a url (or urls) in a popupmenu.
 *
 * This includes:
 * @li "open with <application>" actions, but also
 * @li builtin services like mount/unmount for old-style device desktop files
 * @li user-defined actions for a .desktop file, defined in the file itself (see the desktop entry standard)
 * @li servicemenus actions, defined in .desktop files and selected based on the mimetype of the url
 *
 * KFileItemActions respects Kiosk-based restrictions (see the KAuthorized
 * namespace in the KConfig framework).  In particular, the "action/openwith"
 * action is checked when determining actions for opening files (see
 * addOpenWithActionsTo()) and service-specific actions are checked before
 * adding service actions to a menu (see addServiceActionsTo()).
 *
 * For user-defined actions in a .desktop file, the "X-KDE-AuthorizeAction" key
 * can be used to determine which actions are checked before the user-defined
 * action is allowed.  The action is ignored if any of the listed actions are
 * not authorized.
 *
 * @since 4.3
 */
class KIOWIDGETS_EXPORT KFileItemActions : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a KFileItemActions instance.
     * Note that this instance must stay alive for at least as long as the popupmenu;
     * it has the slots for the actions created by addOpenWithActionsTo/addServiceActionsTo.
     */
    KFileItemActions(QObject *parent = nullptr);

    /**
     * Destructor
     */
    ~KFileItemActions();

    /**
     * Sets all the data for the next instance of the popupmenu.
     * @see KFileItemListProperties
     */
    void setItemListProperties(const KFileItemListProperties &itemList);

    /**
     * Set the parent widget for any dialogs being shown.
     *
     * This should normally be your mainwindow, not a popup menu,
     * so that it still exists even after the popup is closed
     * (e.g. error message from KRun) and so that QAction::setStatusTip
     * can find a statusbar, too.
     */
    void setParentWidget(QWidget *widget);

    /**
     * Generate the "Open With <Application>" actions, and adds them to the @p menu.
     * All actions are created as children of the menu.
     *
     * No actions will be added if the "openwith" Kiosk action is not authorized
     * (see KAuthorized::authorize()).
     *
     * @param menu the QMenu where to add actions
     * @param traderConstraint this constraint allows to exclude the current application
     * from the "open with" list. Example: "DesktopEntryName != 'kfmclient'".
     */
    void addOpenWithActionsTo(QMenu *menu, const QString &traderConstraint = QString());

    /**
     * Returns an action for the preferred application only.
     * @param traderConstraint this constraint allows to exclude the current application
     * from the "open with" list. Example: "DesktopEntryName != 'kfmclient'".
     * @return the action - or @c nullptr if no application was found.
     */
    QAction *preferredOpenWithAction(const QString &traderConstraint);

    /**
     * Returns the applications associated with all the given mimetypes.
     *
     * Helper method used internally, can also be used for similar GUIs that
     * show the list of associated applications.
     * Used in KParts::BrowserOpenOrSaveQuestion for example.
     *
     * This is basically a KMimeTypeTrader::query, but it supports multiple mimetypes, and
     * also cleans up "apparent" duplicates, such as different versions of the same
     * application installed in parallel.
     *
     * The list is sorted according to the user preferences for the given mimetype(s).
     * In case multiple mimetypes appear in the url list, the logic is:
     * applications that on average appear earlier on the associated applications
     * list for the given mimetypes also appear earlier on the final applications list.
     *
     * Note that for a single mimetype there is no need to use this, you should use
     * KMimeTypeTrader instead, e.g. query() or preferredService().
     *
     * This will return an empty list if the "openwith" Kiosk action is not
     * authorized (see KAuthorized::authorize()).
     *
     * @param mimeTypeList the mimetypes
     * @param traderConstraint this optional constraint allows to exclude the current application
     * from the "open with" list. Example: "DesktopEntryName != 'kfmclient'".
     * @return the sorted list of services.
     * @since 4.4
     */
    static KService::List associatedApplications(const QStringList &mimeTypeList, const QString &traderConstraint);

    /**
     * Generate the user-defined actions and submenus, and adds them to the @p menu.
     * User-defined actions include:
     * - builtin services like mount/unmount for old-style device desktop files
     * - user-defined actions for a .desktop file, defined in the file itself (see the desktop entry standard)
     * - servicemenus actions, defined in .desktop files and selected based on the mimetype of the url
     *
     * When KFileItemListProperties::supportsWriting() is false, actions that modify the files are not shown.
     * This is controlled by Require=Write in the servicemenu desktop files.
     *
     * Service actions that are not authorized (see KAuthorized::authorize())
     * are not added.  For user-defined actions in a .desktop file, the
     * "X-KDE-AuthorizeAction" key determines the Kiosk actions that are
     * checked.
     *
     * All actions are created as children of the menu.
     * @return the number of actions added
     */
    int addServiceActionsTo(QMenu *menu);

    /**
     * Add actions implemented by plugins.
     * These are defined in .desktop files or JSON in plugins using the KFileItemAction/Plugin service type,
     * and the KAbstractFileItemActionPlugin base class.
     *
     * All actions are created as children of the menu.
     * @return the number of actions added
     *
     * @since 5.27
     */
    int addPluginActionsTo(QMenu *menu);

Q_SIGNALS:
    /**
     * Emitted before the "Open With" dialog is shown
     * This is used e.g in folderview to close the folder peek popups on invoking the "Open With" menu action
     * @since 4.8.2
     */
    void openWithDialogAboutToBeShown();

public Q_SLOTS:
    /**
     * Slot used to execute a list of files in their respective preferred application.
     * @param fileOpenList the list of KFileItems to open.
     * @param traderConstraint this optional constraint allows to exclude the current application
     * @since 4.5
     */
    void runPreferredApplications(const KFileItemList &fileOpenList, const QString &traderConstraint);

private:
    KFileItemActionsPrivate *const d;
    friend class KFileItemActionsPrivate;
};

#endif /* KFILEITEMACTIONS_H */

