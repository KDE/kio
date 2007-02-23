/* This file is part of the KDE project
 * Copyright (C) 2004 David Faure <faure@kde.org>
 *     Based on kfile_txt.cpp by Nadeem Hasan <nhasan@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "kfile_trash.h"

#include <kgenericfactory.h>
#include <kdebug.h>

#include <QFile>
#include <QStringList>
#include <QDateTime>

typedef KGenericFactory<KTrashPlugin> TrashFactory;

K_EXPORT_COMPONENT_FACTORY(kfile_trash, TrashFactory("kfile_trash"))

KTrashPlugin::KTrashPlugin(QObject *parent, const QStringList &args)
    : KFilePlugin(parent, args)
{
    KGlobal::locale()->insertCatalog( "kio_trash" );

    kDebug(7034) << "Trash file meta info plugin\n";

    makeMimeTypeInfo("trash");
//    makeMimeTypeInfo("system");

    (void)impl.init();
}

void KTrashPlugin::makeMimeTypeInfo(const QString& mimeType)
{
    KFileMimeTypeInfo* info = addMimeTypeInfo( mimeType );

    KFileMimeTypeInfo::GroupInfo* group =
            addGroupInfo(info, "General", i18n("General"));

    KFileMimeTypeInfo::ItemInfo* item;
    item = addItemInfo(group, "OriginalPath", i18n("Original Path"), QVariant::String);
    item = addItemInfo(group, "DateOfDeletion", i18n("Date of Deletion"), QVariant::DateTime);
}

bool KTrashPlugin::readInfo(KFileMetaInfo& info, uint)
{
    KUrl url = info.url();

    if ( url.protocol()=="system"
      && url.path().startsWith("/trash") )
    {
        QString path = url.path();
        path.remove(0, 6);
        url.setProtocol("trash");
        url.setPath(path);
    }
    
    //kDebug() << k_funcinfo << info.url() << endl;
    if ( url.protocol() != "trash" )
        return false;

    int trashId;
    QString fileId;
    QString relativePath;
    if ( !TrashImpl::parseURL( url, trashId, fileId, relativePath ) )
        return false;

    TrashImpl::TrashedFileInfo trashInfo;
    if ( !impl.infoForFile( trashId, fileId, trashInfo ) )
        return false;

    KFileMetaInfoGroup group = appendGroup(info, "General");
    appendItem(group, "OriginalPath", trashInfo.origPath);
    appendItem(group, "DateOfDeletion", trashInfo.deletionDate);

    return true;
}

#include "kfile_trash.moc"
