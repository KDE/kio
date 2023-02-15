/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2015 Olivier Goffart <ogoffart@woboq.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KOVERLAYICONPLUGIN_H
#define KOVERLAYICONPLUGIN_H

#include "kiocore_export.h"
#include <QObject>

class QUrl;

/**
 * @class KOverlayIconPlugin koverlayiconplugin.h <KOverlayIconPlugin>
 *
 * @brief Base class for overlay icon plugins.
 * Enables file managers to show custom overlay icons on files.
 *
 * This plugin can be created and installed through kcoreaddons_add_plugin
 * @code
 * kcoreaddons_add_plugin(myoverlayplugin SOURCES myoverlayplugin.cpp INSTALL_NAMESPACE "kf6/overlayicon")
 * target_link_libraries(myoverlayplugin KF6::KIOCore)
 * @endcode
 * The C++ file should look like this:
 * @code
#include <KOverlayIconPlugin>

class MyOverlayPlugin : public KOverlayIconPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.overlayicon.myplugin")
    Q_OBJECT

public:
    MyOverlayPlugin() {
    }

    QStringList getOverlays(const QUrl &url) override {
        // Implement your logic
    }
};

#include "myoverlayplugin.moc"
 * @endcode
 * @since 5.16
 */
class KIOCORE_EXPORT KOverlayIconPlugin : public QObject
{
    Q_OBJECT
public:
    explicit KOverlayIconPlugin(QObject *parent = nullptr);
    ~KOverlayIconPlugin() override;

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
