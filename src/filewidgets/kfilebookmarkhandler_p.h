/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEBOOKMARKHANDLER_H
#define KFILEBOOKMARKHANDLER_H

#include <KBookmarkManager>
#include <KBookmarkMenu>
#include <KBookmarkOwner>

class QMenu;
class KFileWidget;

class KFileBookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    explicit KFileBookmarkHandler(KFileWidget *widget);
    ~KFileBookmarkHandler() override;

    QMenu *popupMenu();

    // KBookmarkOwner interface:
    QString currentTitle() const override;
    QUrl currentUrl() const override;
    QString currentIcon() const override;

    QMenu *menu() const
    {
        return m_menu;
    }

public Q_SLOTS:
    void openBookmark(const KBookmark &bm, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers) override;

Q_SIGNALS:
    void openUrl(const QString &url);

private:
    void importOldBookmarks(const QString &path, KBookmarkManager *manager);

    KFileWidget *m_widget;
    QMenu *m_menu;
    KBookmarkMenu *m_bookmarkMenu;

private:
    class KFileBookmarkHandlerPrivate;
    KFileBookmarkHandlerPrivate *d;
};

#endif // KFILEBOOKMARKHANDLER_H
