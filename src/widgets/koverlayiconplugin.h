/* This file is part of the KDE project
   Copyright (C) 2015 Olivier Goffart <ogoffart@woboq.com>

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

#ifndef KOVERLAYICONPLUGIN_H
#define KOVERLAYICONPLUGIN_H

#include "kiowidgets_export.h"
#include <QObject>

class QUrl;

/**
 * @class KOverlayIconPlugin koverlayiconplugin.h <KOverlayIconPlugin>
 *
 * @brief Base class for overlay icon plugins.
 *
 * Enables the file manager to show custom overlay icons on files.
 *
 * To write a custom plugin you need to create a .desktop file for your plugin with
 * X-KDE-ServiceTypes=KOverlayIconPlugin
 *
 * @since 5.16
 */
class KIOWIDGETS_EXPORT KOverlayIconPlugin : public QObject
{
    Q_OBJECT
public:
    explicit KOverlayIconPlugin(QObject *parent = nullptr);
    ~KOverlayIconPlugin();

    /**
     * Returns a list of overlay icons to add to a file
     * This can be a path to an icon, or the icon name
     *
     * This function is called from the main thread and must not block.
     * It is recommended to have a cache. And if the item is not in cache
     * just return an empty list and call the overlaysChanged when the
     * information is available.
     */
    virtual QStringList getOverlays(const QUrl &item) = 0;
Q_SIGNALS:
    /**
     * Emit this signal when the list of overlay icons changed for a given URL
     */
    void overlaysChanged(const QUrl &url, const QStringList &overlays);
};

#endif
