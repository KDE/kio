/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2015 Olivier Goffart <ogoffart@woboq.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
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
