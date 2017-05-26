/*  This file is part of the KDE project
    Copyright 2009  Harald Hvaal <haraldhv@stud.ntnu.no>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) version 3.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef _KIO_DNDPOPUPMENUPLUGIN_H_
#define _KIO_DNDPOPUPMENUPLUGIN_H_

#include "kiowidgets_export.h"
#include <QObject>
#include <QList>

class KFileItemListProperties;
class QUrl;
class QAction;

namespace KIO
{
/**
 * @class KIO::DndPopupMenuPlugin dndpopupmenuplugin.h <KIO/DndPopupMenuPlugin>
 *
 * Base class for drag and drop popup menus
 *
 * This can be used for adding dynamic menu items to the normal copy/move/link
 * here menu appearing in KIO-based file managers. In the setup method you may check
 * the properties of the dropped files, and if applicable, append your own
 * QAction that the user may trigger in the menu.
 *
 * The plugin should have Json metadata and be installed into kf5/kio_dnd/.
 *
 * @author Harald Hvaal <metellius@gmail.com>
 * @since 5.6
 */
class KIOWIDGETS_EXPORT DndPopupMenuPlugin : public QObject
{
    Q_OBJECT
public:

    /**
     * Constructor.
     */
    DndPopupMenuPlugin(QObject* parent);
    virtual ~DndPopupMenuPlugin();

    /**
     * Implement the setup method in the plugin in order to create actions
     * in the given actionCollection and add it to the menu using menu->addAction().
     * The popup menu will be set as parent of the actions.
     *
     * @param popupMenuInfo all the information about the source URLs being dropped
     * @param destination the URL to where the file(s) were dropped
     * @return a QList with the QActions that will be plugged into the menu.
     */
    virtual QList<QAction *> setup(const KFileItemListProperties& popupMenuInfo,
                                   const QUrl& destination) = 0;
};

}

#endif
