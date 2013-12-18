/* This file is part of the KDE project
   Copyright (C) 2010 Sebastian Trueg <trueg@kde.org>
   Based on konq_popupmenuplugin.h Copyright 2008 David Faure <faure@kde.org>

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

#ifndef KABSTRACTFILEITEMACTION_PLUGIN_H
#define KABSTRACTFILEITEMACTION_PLUGIN_H

#include <kio/kiowidgets_export.h>
#include <QtCore/QObject>

class QAction;
class QMenu;
class QWidget;
class KFileItemListProperties;

/**
 * @brief Base class for KFileItemAction plugins.
 *
 * Please try to use servicemenus first, if you simply need to add
 * actions to the popup menu for one or more mimetypes.
 *
 * However if you need some dynamic logic, like "only show this item if
 * two files are selected", or "show a submenu with a variable number of actions",
 * then you have to implement a KAbstractFileItemActionPlugin subclass.
 *
 * Create a KPluginFactory instance and register your plugin as return type:
 *
 * \code
 * K_PLUGIN_FACTORY(MyActionPluginFactory, registerPlugin<MyActionPlugin>();)
 * \endcode
 *
 * You can compile the metadata into the plugin using
 * \code
 * K_PLUGIN_FACTORY(MyActionPluginFactory, myactionplugin.json, registerPlugin<MyActionPlugin>();)
 * \endcode
 *
 * A desktop file is necessary to register the plugin with the KDE plugin system:
 *
 * \code
 * [Desktop Entry]
 * Encoding=UTF-8
 * Type=Service
 * Name=My fancy action plugin
 * X-KDE-Library=myactionplugin
 * ServiceTypes=KFileItemAction/Plugin
 * MimeType=some/mimetype;
 * \endcode
 *
 * Note the \p KFileItemAction/Plugin service type which is used by
 * KFileItemActions::addServicePluginActionsTo() to load all available plugins
 * and the \p MimeType field which specifies for which types of file items
 * the setup() method should be called.
 *
 *
 * As with all KDE plugins one needs to install the plugin as a module. In
 * cmake terms this looks roughly as follows:
 *
 * \code
 * desktop_to_json(myactionplugin.desktop) # generate the json file
 *
 * add_library(myactionplugin MODULE ${myactionplugin_SRCS} )
 * set_target_properties(myactionplugin PROPERTIES PREFIX "") # remove lib prefix from binary
 *
 * target_link_libraries(myactionplugin ${KDE4_KIO_LIBS})
 * install(TARGETS myactionplugin DESTINATION ${PLUGIN_INSTALL_DIR})
 * install(FILES myactionplugin.desktop DESTINATION ${SERVICES_INSTALL_DIR})
 * \endcode
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
     * @param fileItemInfos The information about the selected file items.
     * (Which file items, their common mimetype, etc.)
     * @param parentWidget A parent widget for error messages or the like.
     * @return List of actions, that should added to e. g. the popup menu.
     *         It is recommended to use the KAbstractFileItemActionPlugin as parent
     *         of the actions.
     */
    virtual QList<QAction*> actions(const KFileItemListProperties &fileItemInfos,
                                    QWidget *parentWidget) = 0;
};

#endif
