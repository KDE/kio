/* Copyright 2008, 2015 David Faure <faure@kde.org>

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

#ifndef KFILECOPYTOMENU_H
#define KFILECOPYTOMENU_H

#include <QUrl>
#include <QObject>

#include <kiofilewidgets_export.h>

class QMenu;
class KFileCopyToMenuPrivate;

/**
 * @class KFileCopyToMenu kfilecopytomenu.h <KFileCopyToMenu>
 *
 * This class adds "Copy To" and "Move To" submenus to a popupmenu.
 */
class KIOFILEWIDGETS_EXPORT KFileCopyToMenu : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a KFileCopyToMenu instance
     * Note that this instance (and the widget) must stay alive for at least as
     * long as the popupmenu; it has the slots for the actions created by addActionsTo.
     *
     * @param parentWidget parent widget for the file dialog and message boxes.
     * The parentWidget also serves as a parent for this object.
     */
    explicit KFileCopyToMenu(QWidget *parentWidget);

    /**
     * Destructor
     */
    ~KFileCopyToMenu();

    /**
     * Sets the URLs which the actions apply to.
     */
    void setUrls(const QList<QUrl> &urls);

    /**
     * If setReadOnly(true) is called, the "Move To" submenu will not appear.
     */
    void setReadOnly(bool ro);

    /**
     * Generate the actions and submenus, and adds them to the @p menu.
     * All actions are created as children of the menu.
     */
    void addActionsTo(QMenu *menu) const;

    /**
     * Enables or disables automatic error handling with message boxes.
     * When called with true, a messagebox is shown in case of an error during a copy or move.
     * When called with false, the application should connect to the error signal instead.
     * Auto error handling is disabled by default.
     */
    void setAutoErrorHandlingEnabled(bool b);

Q_SIGNALS:
    /**
     * Emitted when the copy or move job fails.
     * @param errorCode the KIO job error code
     * @param message the error message to show the user
     */
    void error(int errorCode, const QString &message);

private:
    KFileCopyToMenuPrivate *const d;
};

#endif
