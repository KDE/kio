/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2009 Harald Hvaal <haraldhv@stud.ntnu.no>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
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
