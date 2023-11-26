/*
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "kiowidgets_export.h"
#include <QObject>

#ifndef KPROPERTIESDIALOGPLUGIN_H
#define KPROPERTIESDIALOGPLUGIN_H

#include "kiowidgets_export.h"
#include <QObject>
#include <kpropertiesdialog.h>

class KPropertiesDialogPluginPrivate;
/**
 * A Plugin in the Properties dialog
 * This is an abstract class. You must inherit from this class
 * to build a new kind of tabbed page for the KPropertiesDialog.
 * A plugin in itself is just a library containing code, not a dialog's page.
 * It's up to the plugin to insert pages into the parent dialog.
 *
 * To make a plugin available, ensure it has embedded json metadata using
 * K_PLUGIN_CLASS_WITH_JSON and install the plugin in the KDE_INSTALL_PLUGINDIR/kf6/propertiesdialog
 * folder from the KDEInstallDirs CMake module.
 *
 * The metadata can contain the MIME types for which the plugin should be created.
 * For instance:
 * @verbatim
   {
       "KPlugin": {
           "MimeTypes": ["text/html", "application/x-mymimetype"]
       },
       "X-KDE-Protocols": ["file"]
   }
   @endverbatim
 * If the MIME types are empty or not specified, the plugin will be created for all MIME types.
 *
 * You can also include "X-KDE-Protocols" if you want that plugin for instance
 * to be loaded only for local files.
 */
class KIOWIDGETS_EXPORT KPropertiesDialogPlugin : public QObject
{
    Q_OBJECT
public:
    /**
     * Constructor whos parent will be cast to KPropertiesDialog
     * To insert tabs into the properties dialog, use the add methods provided by
     * KPageDialog (the properties dialog is a KPageDialog).
     */
    KPropertiesDialogPlugin(QObject *parent);
    ~KPropertiesDialogPlugin() override;

    /**
     * Applies all changes to the file.
     * This function is called when the user presses 'Ok'. The last plugin inserted
     * is called first.
     */
    virtual void applyChanges();

    void setDirty(bool b = true);
    bool isDirty() const;

Q_SIGNALS:
    /**
     * Emit this signal when the user changed anything in the plugin's tabs.
     * The hosting PropertiesDialog will call applyChanges only if the
     * PropsPlugin has emitted this signal or if you have called setDirty() before.
     */
    void changed();

protected:
    /**
     * Pointer to the dialog
     */
    KPropertiesDialog *const properties;

    /**
     * Returns the font height.
     */
    int fontHeight() const;

private:
    const std::unique_ptr<KPropertiesDialogPluginPrivate> d;
};
#endif
