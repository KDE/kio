/*
    SPDX-FileCopyrightText: 2015 Montel Laurent <montel@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
