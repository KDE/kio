/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KFILEITEMACTIONS_H
#define KFILEITEMACTIONS_H

#include "kiowidgets_export.h"
#include <KService>
#include <kfileitem.h>

#include <memory>

class KFileItemListProperties;
class QAction;
class QMenu;
class KFileItemActionsPrivate;

/*!
 * \class KFileItemActions
 * \inmodule KIOWidgets
 *
 * This class creates and handles the actions for a url (or urls) in a popupmenu.
 *
 * This includes:
 * \list
 * \li "open with <application>" actions, but also
 * \li user-defined actions for a .desktop file, defined in the file itself (see the desktop entry standard)
 * \li servicemenus actions, defined in .desktop files and selected based on the MIME type of the url
 * \endlist
 *
 * KFileItemActions respects Kiosk-based restrictions (see the KAuthorized
 * namespace in the KConfig framework). In particular, the "action/openwith"
 * action is checked when determining actions for opening files (see
 * addOpenWithActionsTo()) and service-specific actions are checked before
 * adding service actions to a menu (see addServiceActionsTo()).
 *
 * For user-defined actions in a .desktop file, the "X-KDE-AuthorizeAction" key
 * can be used to determine which actions are checked before the user-defined
 * action is allowed.  The action is ignored if any of the listed actions are
 * not authorized.
 *
 * \note The builtin services like mount/unmount for old-style device desktop
 * files (which mainly concerns CDROM and Floppy drives) have been deprecated
 * since 5.82; those menu entries were hidden long before that, since the FSDevice
 * .desktop template file hadn't been installed for quite a while.
 */
class KIOWIDGETS_EXPORT KFileItemActions : public QObject
{
    Q_OBJECT
public:
    /*!
     * Creates a KFileItemActions instance.
     * Note that this instance must stay alive for at least as long as the popupmenu;
     * it has the slots for the actions created by addOpenWithActionsTo/addServiceActionsTo.
     */
    KFileItemActions(QObject *parent = nullptr);

    ~KFileItemActions() override;

    /*!
     * Sets all the data for the next instance of the popupmenu.
     * \sa KFileItemListProperties
     */
    void setItemListProperties(const KFileItemListProperties &itemList);

    /*!
     * Set the parent widget for any dialogs being shown.
     *
     * This should normally be your mainwindow, not a popup menu,
     * so that it still exists even after the popup is closed
     * (e.g. error message from KRun) and so that QAction::setStatusTip
     * can find a statusbar, too.
     */
    void setParentWidget(QWidget *widget);

    /*!
     * Generates the "Open With <Application>" actions, and inserts them in \a menu,
     * before action \a before. If \a before is nullptr or doesn't exist in the menu
     * the actions will be appended to the menu.
     *
     * All actions are created as children of the menu.
     *
     * No actions will be added if the "openwith" Kiosk action is not authorized
     * (see KAuthorized::authorize()).
     *
     * \a before the "open with" actions will be inserted before this action; if this action
     * is nullptr or isn't available in \a topMenu, the "open with" actions will be appended
     *
     * \a menu the QMenu where the actions will be added
     *
     * \a excludedDesktopEntryNames list of desktop entry names that will not be shown
     *
     * \since 5.82
     */
    void insertOpenWithActionsTo(QAction *before, QMenu *topMenu, const QStringList &excludedDesktopEntryNames);

    /*!
     * Returns the applications associated with all the given MIME types.
     *
     * This is basically a KApplicationTrader::query, but it supports multiple MIME types, and
     * also cleans up "apparent" duplicates, such as different versions of the same
     * application installed in parallel.
     *
     * The list is sorted according to the user preferences for the given MIME type(s).
     * In case multiple MIME types appear in the URL list, the logic is:
     * applications that on average appear earlier on the associated applications
     * list for the given MIME types also appear earlier on the final applications list.
     *
     * Note that for a single MIME type there is no need to use this, you should use
     * KApplicationTrader instead, e.g. query() or preferredService().
     *
     * This will return an empty list if the "openwith" Kiosk action is not
     * authorized (see KAuthorized::authorize()).
     *
     * \a mimeTypeList the MIME types
     *
     * Returns the sorted list of services.
     *
     * \since 5.83
     */
    static KService::List associatedApplications(const QStringList &mimeTypeList);

    /*!
     * \value Services Add user defined actions and servicemenu actions (this used to include builtin actions, which have been deprecated since 5.82 see class
     * API documentation)
     * \value Plugins Add actions implemented by plugins. See KAbstractFileItemActionPlugin base class
     * \value All
     *
     */
    enum class MenuActionSource {
        Services = 0x1,
        Plugins = 0x2,
        All = Services | Plugins,
    };
    Q_DECLARE_FLAGS(MenuActionSources, MenuActionSource)

    /*!
     * This methods adds additional actions to the menu.
     *
     * \a menu Menu to which the actions/submenus will be added.
     *
     * \a sources sources from which the actions should be fetched. By default all sources are used.
     *
     * \a additionalActions additional actions that should be added to the "Actions" submenu or
     * top level menu if there are less than three entries in total.
     *
     * \a excludeList list of action names or plugin ids that should be excluded
     *
     * \since 5.77
     */
    void addActionsTo(QMenu *menu,
                      MenuActionSources sources = MenuActionSource::All,
                      const QList<QAction *> &additionalActions = {},
                      const QStringList &excludeList = {});

Q_SIGNALS:
    /*!
     * Emitted before the "Open With" dialog is shown
     * This is used e.g in folderview to close the folder peek popups on invoking the "Open With" menu action
     * \since 4.8.2
     */
    void openWithDialogAboutToBeShown();

    /*!
     * Forwards the errors from the KAbstractFileItemActionPlugin instances
     * \since 5.82
     */
    void error(const QString &errorMessage);

public Q_SLOTS:
    /*!
     * Slot used to execute a list of files in their respective preferred application.
     *
     * \a fileOpenList the list of KFileItems to open.
     *
     * \since 5.83
     */
    void runPreferredApplications(const KFileItemList &fileOpenList);

private:
    std::unique_ptr<KFileItemActionsPrivate> const d;
    friend class KFileItemActionsPrivate;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(KFileItemActions::MenuActionSources)

#endif /* KFILEITEMACTIONS_H */
