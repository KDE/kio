/* This file is part of the KDE project
 * Copyright (C) 2004 David Faure <faure@kde.org>
 *     Based on kfile_txt.h by Nadeem Hasan <nhasan@kde.org>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef __KFILE_TRASH_H_
#define __KFILE_TRASH_H_

#include <kfilemetainfo.h>
#include "../trashimpl.h"

class QStringList;

class KTrashPlugin: public KFilePlugin
{
    Q_OBJECT

public:
    KTrashPlugin(QObject *parent, const char *name, const QStringList& args);
    virtual bool readInfo(KFileMetaInfo& info, uint what);

private:
    void makeMimeTypeInfo(const QString& mimeType);
    TrashImpl impl;
};

#endif
