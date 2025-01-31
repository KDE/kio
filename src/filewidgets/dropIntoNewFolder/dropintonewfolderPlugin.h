/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2025 Méven Car <meven@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DROPINTONEWFOLDERPLUGIN_H
#define DROPINTONEWFOLDERPLUGIN_H

#include <KIO/DndPopupMenuPlugin>

#include <QUrl>

class DropIntoNewFolderPlugin : public KIO::DndPopupMenuPlugin
{
    Q_OBJECT

public:
    DropIntoNewFolderPlugin(QObject *parent, const QVariantList &);

    QList<QAction *> setup(const KFileItemListProperties &fileItemProps, const QUrl &destination) override;

private Q_SLOTS:
    void slotTriggered();

private:
    QUrl m_dest;
    QList<QUrl> m_urls;
};

#endif /* DROPINTONEWFOLDERPLUGIN_H */
