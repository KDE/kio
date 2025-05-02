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

/*!
 * \class KAbstractFileItemActionPlugin
 * \inmodule KIOWidgets
 *
 * \brief Base class for KFileItemAction plugins.
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
 * Note the KFileItemAction/Plugin service type which is used by
 * KFileItemActions::addServicePluginActionsTo() to load all available plugins
 * and the MimeType field which specifies for which types of file items
 * the setup() method should be called.
 *
 * The desktop file contents must also be compiled into the plugin as JSON data.
 * The following CMake code builds and installs the plugin:
 * \code
 * kcoreaddons_add_plugin(myactionplugin SOURCES myactionplugin.cpp INSTALL_NAMESPACE "kf6/kfileitemaction")
 * kcoreaddons_desktop_to_json(myactionplugin myactionplugin.desktop) # generate the json file if necessary
 *
 * target_link_libraries(myactionplugin KF5::KIOWidgets)
 * \endcode
 *
 * \note The plugin should be installed in the "kf5/kfileitemaction" subfolder of $QT_PLUGIN_PATH.
 *
 * \note If the plugin has a lower priority and should show up in the "Actions" submenu,
 * you can set the X-KDE-Show-In-Submenu property to true.
 *
 * \since 4.6.1
 */
class KIOWIDGETS_EXPORT KAbstractFileItemActionPlugin : public QObject
{
    Q_OBJECT

public:
    /*!
     *
     */
    explicit KAbstractFileItemActionPlugin(QObject *parent);

    ~KAbstractFileItemActionPlugin() override;

    // TODO KF7 make this asynchronous and stoppable, so a bad plugin cannot impact too much the application process
    // KIO could enforce a timeout and run it in a Thread
    /*!
     * Implement the actions method in the plugin in order to create actions.
     *
     * \a fileItemInfos  Information about the selected file items.
     *
     * \a parentWidget   To be used as parent for the returned QActions
     *
     * Returns a list of actions to be added to a contextual menu for the file items.
     */
    virtual QList<QAction *> actions(const KFileItemListProperties &fileItemInfos, QWidget *parentWidget) = 0;

Q_SIGNALS:
    /*!
     * Emits an error which will be displayed to the user
     * \since 5.82
     */
    void error(const QString &errorMessage);
};

#endif
