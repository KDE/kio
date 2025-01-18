/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2025 Méven Car <meven@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "dropnewintonewfolderPlugin.h"
#include "copyjob.h"
#include "openfilemanagerwindowjob.h"
#include <KIO/FileUndoManager>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/StatJob>
#include <kfileitem.h>
#include <knewfilemenu.h>

#include <QAction>

#include <KFileItemListProperties>
#include <KLocalizedString>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(DropIntoNewFolderPlugin, "dropnewintonewfolder.json")

DropIntoNewFolderPlugin::DropIntoNewFolderPlugin(QObject *parent, const QVariantList &)
    : KIO::DndPopupMenuPlugin(parent)
{
}

QList<QAction *> DropIntoNewFolderPlugin::setup(const KFileItemListProperties &fileItemProps, const QUrl &destination)
{
    QList<QAction *> actionList;

    if (!destination.isLocalFile()) {
        return actionList;
    }
    // need to check write acccess to dest and m_urls
    bool allowed = fileItemProps.supportsMoving();

    if (allowed) {
        auto statJob = KIO::stat(destination, KIO::StatJob::StatSide::SourceSide, KIO::StatDetail::StatBasic);

        if (!statJob->exec()) {
            qWarning() << "Could not stat destinantion" << destination;
            allowed = false;
        } else {
            KFileItem item(statJob->statResult(), destination);
            allowed = item.isWritable();
        }
    }

    const QString dropIntoNewFolderMessage = i18nc("@action:inmenu Context menu shown when files are dragged", "Move Selection Into New Folder");

    QAction *action = new QAction(QIcon::fromTheme(QStringLiteral("folder-new")), dropIntoNewFolderMessage, this);
    connect(action, &QAction::triggered, this, &DropIntoNewFolderPlugin::slotTriggered);
    action->setEnabled(allowed);

    actionList.append(action);
    m_dest = destination;
    m_urls = fileItemProps.urlList();

    return actionList;
}

void DropIntoNewFolderPlugin::slotTriggered()
{
    auto menu = new KNewFileMenu(this);
    menu->setWorkingDirectory(m_dest);
    menu->setWindowTitle(i18nc("@title:window", "Create New Folder to move selection into"));

    connect(menu, &KNewFileMenu::directoryCreated, this, [this](const QUrl &newFolder) {
        auto job = KIO::move(m_urls, newFolder);
        KIO::FileUndoManager::self()->recordCopyJob(job);
        connect(job, &KJob::result, this, [newFolder, this](KJob *job) {
            if (job->error() != KJob::NoError) {
                return;
            }
            auto openFileManagerJob = new KIO::OpenFileManagerWindowJob{this};
            openFileManagerJob->setHighlightUrls({newFolder});
            openFileManagerJob->start();
        });
        job->start();
    });

    menu->createDirectory();
}

#include "dropnewintonewfolderPlugin.moc"
#include "moc_dropnewintonewfolderPlugin.cpp"
