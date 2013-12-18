/* This file is part of the KDE libraries
   Copyright (C) 2000-2005 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "metadata.h"

KIO::MetaData::MetaData(const QMap<QString,QVariant>& map)
{
    *this = map;
}

KIO::MetaData & KIO::MetaData::operator += ( const QMap<QString,QVariant> &metaData )
{
    QMapIterator<QString,QVariant> it (metaData);

    while(it.hasNext()) {
        it.next();
        insert(it.key(), it.value().toString());
    }

    return *this;
}

KIO::MetaData & KIO::MetaData::operator = ( const QMap<QString,QVariant> &metaData )
{
    clear();
    return (*this += metaData);
}

QVariant KIO::MetaData::toVariant() const
{
    QMap<QString, QVariant> map;
    QMapIterator <QString,QString> it (*this);

    while (it.hasNext()) {
        it.next();
        map.insert(it.key(), it.value());
    }

    return QVariant(map);
}
