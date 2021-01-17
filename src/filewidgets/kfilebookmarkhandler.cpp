/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfilebookmarkhandler_p.h"

#include <stdio.h>
#include <stdlib.h>

#include <QMenu>

#include <kbookmarkimporter.h>
#include <KBookmarkDomBuilder>
#include <kio/global.h>
#include <QStandardPaths>

#include "kfilewidget.h"

KFileBookmarkHandler::KFileBookmarkHandler(KFileWidget *widget)
    : QObject(widget),
      KBookmarkOwner(),
      m_widget(widget)
{
    setObjectName(QStringLiteral("KFileBookmarkHandler"));
    m_menu = new QMenu(widget);
    m_menu->setObjectName(QStringLiteral("bookmark menu"));

    QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kfile/bookmarks.xml"));
    if (file.isEmpty()) {
        file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kfile/bookmarks.xml");
    }

    KBookmarkManager *manager = KBookmarkManager::managerForFile(file, QStringLiteral("kfile"));
    manager->setUpdate(true);

    m_bookmarkMenu = new KBookmarkMenu(manager, this, m_menu);
}

KFileBookmarkHandler::~KFileBookmarkHandler()
{
    delete m_bookmarkMenu;
}

void KFileBookmarkHandler::openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers)
{
    Q_EMIT openUrl(bm.url().toString());
}

QUrl KFileBookmarkHandler::currentUrl() const
{
    return m_widget->baseUrl();
}

QString KFileBookmarkHandler::currentTitle() const
{
    return m_widget->baseUrl().toDisplayString();
}

QString KFileBookmarkHandler::currentIcon() const
{
    return KIO::iconNameForUrl(currentUrl());
}

#include "moc_kfilebookmarkhandler_p.cpp"
