/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2010 Sebastian Trueg <trueg@kde.org>

    Based on konq_popupmenuplugin.h
    SPDX-FileCopyrightText: 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KABSTRACTFILEITEMACTION_PLUGIN_H
#define KABSTRACTFILEITEMACTION_PLUGIN_H

#include "kiowidgets_export.h"
#include <QObject>

class QAction;
class QMenu;
class QWidget;
class KFileItemListProperties;

/**
 * @class KAbstractFileItemActionPlugin kabstractfileitemactionplugin.h <KAbstractFileItemActionPlugin>
 *
 * @brief Base class for KFileItemAction plugins.
 *
 * KFileItemAction plugins allow dynamic features to be added to the context
 * menus for files and directories when browsing.
 *
 * Most filetype-based popup menu items can be implemented using servicemenus
 * linked to MIME types, and that should be the preferred way of doing this.
 * However, complex scenarios such as showing submenus with a variable number of
 * actions or only showing an item if exactly two files are selected need to be
 * implemented as a KFileItemAction plugin.
 *
 * To create such a plugin, subclass KAbstractFileItemActionPlugin and implement
 * actions() to return the actions to want to add to the context menu.  Then
 * create a plugin in the usual KPluginFactory based way:
 * \code
 * K_PLUGIN_CLASS_WITH_JSON(MyActionPlugin, "myactionplugin.json")
 * #include <thisfile.moc>
 * \endcode
 *
 * A desktop file is necessary to register the plugin with the KDE plugin system:
 *
 * \code
 * [Desktop Entry]
 * Type=Service
 * Name=My fancy action plugin
 * X-KDE-Library=myactionplugin
 * X-KDE-ServiceTypes=KFileItemAction/Plugin
 * MimeType=some/mimetype;
 * \endcode
 *
 * Note the \p KFileItemAction/Plugin service type which is used by
 * KFileItemActions::addServicePluginActionsTo() to load all available plugins
 * and the \p MimeType field which specifies for which types of file items
 * the setup() method should be called.
 *
 * The desktop file contents must also be compiled into the plugin as JSON data.
 * The following CMake code builds and installs the plugin:
 * \code
 * set(myactionplugin_SRCS myactionplugin.cpp)
 *
 * kcoreaddons_add_plugin(myactionplugin SOURCES ${myactionplugin_SRCS} INSTALL_NAMESPACE "kf5/kfileitemaction")
 * kcoreaddons_desktop_to_json(myactionplugin myactionplugin.desktop) # generate the json file
 *
 * target_link_libraries(myactionplugin KF5::KIOWidgets)
 * \endcode
 *
 * @note the plugin should be installed in the "kf5/kfileitemaction" subfolder of $QT_PLUGIN_PATH.
 * @note If the plugin has a lower priority and should show up in the "Actions" submenu,
 * you can set the X-KDE-Show-In-Submenu property to true.
 *
 * @author Sebastian Trueg <trueg@kde.org>
 *
 * @since 4.6.1
 */
class KIOWIDGETS_EXPORT KAbstractFileItemActionPlugin : public QObject
{
    Q_OBJECT

public:
    KAbstractFileItemActionPlugin(QObject *parent);

    virtual ~KAbstractFileItemActionPlugin();

    /**
     * Implement the actions method in the plugin in order to create actions.
     *
     * The returned actions should have the KAbstractFileItemActionPlugin object
     * as their parent.
     *
     * @param fileItemInfos  Information about the selected file items.
     * @param parentWidget   To be used as parent for the returned QActions and error messages or the like.
     *
     * @return A list of actions to be added to a contextual menu for the file
     *         items.
     */
    virtual QList<QAction *> actions(const KFileItemListProperties &fileItemInfos,
                                     QWidget *parentWidget) = 0;
};

#endif
