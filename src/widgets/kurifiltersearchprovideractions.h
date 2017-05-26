/*
    Copyright (c) 2015 Montel Laurent <montel@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KURIFILTERSEARCHPROVIDERACTIONS_H
#define KURIFILTERSEARCHPROVIDERACTIONS_H

#include <QObject>
#include "kiowidgets_export.h"


class QMenu;
class QAction;
namespace KIO
{
class WebShortcutsMenuManagerPrivate;
/**
 * @class KUriFilterSearchProviderActions kurifiltersearchprovideractions.h <KIO/KUriFilterSearchProviderActions>
 *
 * This class is a manager for web shortcuts
 *
 * It will provide a list of web shortcuts against a selected text
 *
 * You can set the selected text with setSelectedText() function
 *
 * @since 5.16
 */
class KIOWIDGETS_EXPORT KUriFilterSearchProviderActions : public QObject
{
    Q_OBJECT
public:
    /**
     * Constructs a webshorts menu manager.
     *
     * @param parent The QObject parent.
     */

    explicit KUriFilterSearchProviderActions(QObject *parent = nullptr);
    ~KUriFilterSearchProviderActions();

    /**
     * @brief return the selected text
     */
    QString selectedText() const;
    /**
     * @brief Set selected text
     * @param selectedText the text to search for
     */
    void setSelectedText(const QString &selectedText);

    /**
     * @brief addWebShortcutsToMenu Manage to add web shortcut actions to existing menu.
     * @param menu menu to add shortcuts to
     */
    void addWebShortcutsToMenu(QMenu *menu);

private Q_SLOTS:
    void slotConfigureWebShortcuts();
    void slotHandleWebShortcutAction(QAction *action);

private:
    WebShortcutsMenuManagerPrivate *const d;
};
}

#endif // WEBSHORTCUTSMENUMANAGER_H
