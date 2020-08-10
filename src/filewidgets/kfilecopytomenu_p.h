/*
    SPDX-FileCopyrightText: 2008, 2015 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KFILECOPYTOMENU_P_H
#define KFILECOPYTOMENU_P_H

#include <KConfigGroup>
#include <QMenu>
#include <QActionGroup>
#include <QUrl>

class KFileCopyToMenuPrivate
{
public:
    KFileCopyToMenuPrivate(KFileCopyToMenu *qq, QWidget *parentWidget);

    KFileCopyToMenu * const q;
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
