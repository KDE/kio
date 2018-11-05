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

#ifndef KFILECOPYTOMENU_P_H
#define KFILECOPYTOMENU_P_H

#include <kconfiggroup.h>
#include <QMenu>
#include <QActionGroup>
#include <QObject>
#include <QUrl>

class KFileCopyToMenuPrivate
{
public:
    KFileCopyToMenuPrivate(KFileCopyToMenu *qq, QWidget *parentWidget);

    KFileCopyToMenu *q;
    QList<QUrl> m_urls;
    QWidget *m_parentWidget;
    bool m_readOnly;
    bool m_autoErrorHandling;
};

enum MenuType { Copy, Move };

// The main menu, shown when opening "Copy To" or "Move To"
// It contains Home Folder, Root Folder, Browse, and recent destinations
class KFileCopyToMainMenu : public QMenu
{
    Q_OBJECT
public:
    KFileCopyToMainMenu(QMenu *parent, KFileCopyToMenuPrivate *d, MenuType menuType);

    QActionGroup &actionGroup()
    {
        return m_actionGroup;    // used by submenus
    }
    MenuType menuType() const
    {
        return m_menuType;    // used by submenus
    }

private Q_SLOTS:
    void slotAboutToShow();
    void slotBrowse();
    void slotTriggered(QAction *action);

private:
    void copyOrMoveTo(const QUrl &dest);

private:
    MenuType m_menuType;
    QActionGroup m_actionGroup;
    KFileCopyToMenuPrivate *d; // this isn't our own d pointer, it's the one for the public class
    KConfigGroup m_recentDirsGroup;
};

// The menu that lists a directory
class KFileCopyToDirectoryMenu : public QMenu
{
    Q_OBJECT
public:
    KFileCopyToDirectoryMenu(QMenu *parent, KFileCopyToMainMenu *mainMenu, const QString &path);

private Q_SLOTS:
    void slotAboutToShow();

private:
    KFileCopyToMainMenu *m_mainMenu;
    QString m_path;
};

#endif
