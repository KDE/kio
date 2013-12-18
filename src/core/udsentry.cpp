/* This file is part of the KDE project
   Copyright (C) 2000-2005 David Faure <faure@kde.org>
   Copyright (C) 2007 Norbert Frese <nf2@scheinwelt.at>
   Copyright (C) 2007 Thiago Macieira <thiago@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "udsentry.h"

#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QDataStream>
#include <QtCore/QVector>

#include <qplatformdefs.h>

using namespace KIO;

/* ---------- UDSEntry ------------ */

class KIO::UDSEntryPrivate : public QSharedData
{
public:
    struct Field
    {
        inline Field() : m_long(0) { }
        QString m_str;
        long long m_long;
    };
    typedef QHash<uint, Field> FieldHash;
    FieldHash fields;

    static void save(QDataStream &, const UDSEntry &);
    static void load(QDataStream &, UDSEntry &);
};
Q_DECLARE_TYPEINFO(KIO::UDSEntryPrivate::Field, Q_MOVABLE_TYPE);

UDSEntry::UDSEntry()
    : d(new UDSEntryPrivate())
{
}

UDSEntry::UDSEntry(const UDSEntry &other)
    : d(other.d)
{
}

UDSEntry::~UDSEntry()
{
}

UDSEntry &UDSEntry::operator=(const UDSEntry &other)
{
    d = other.d;
    return *this;
}

QString UDSEntry::stringValue(uint field) const
{
    return d->fields.value(field).m_str;
}

long long UDSEntry::numberValue(uint field, long long defaultValue) const
{
    UDSEntryPrivate::FieldHash::ConstIterator it = d->fields.find(field);
    return it != d->fields.constEnd() ? it->m_long : defaultValue;
}

bool UDSEntry::isDir() const
{
    return (numberValue(UDS_FILE_TYPE) & QT_STAT_MASK) == QT_STAT_DIR;
}

bool UDSEntry::isLink() const
{
    return !stringValue(UDS_LINK_DEST).isEmpty();
}

void UDSEntry::insert(uint field, const QString& value)
{
    UDSEntryPrivate::Field f;
    f.m_str = value;
    d->fields.insert(field, f);
}

void UDSEntry::insert(uint field, long long value)
{
    UDSEntryPrivate::Field f;
    f.m_long = value;
    d->fields.insert(field, f);
}

QList<uint> UDSEntry::listFields() const
{
    return d->fields.keys();
}

int UDSEntry::count() const
{
    return d->fields.count();
}

bool UDSEntry::contains(uint field) const
{
    return d->fields.contains(field);
}

bool UDSEntry::remove(uint field)
{
    return d->fields.remove(field) > 0;
}

void UDSEntry::clear()
{
    d->fields.clear();
}

QDataStream & operator<<(QDataStream &s, const UDSEntry &a)
{
    UDSEntryPrivate::save(s, a);
    return s;
}

QDataStream & operator>>(QDataStream &s, UDSEntry &a)
{
    UDSEntryPrivate::load(s, a);
    return s;
}

void UDSEntryPrivate::save(QDataStream &s, const UDSEntry &a)
{
    const FieldHash &e = a.d->fields;

    s << e.size();
    FieldHash::ConstIterator it = e.begin();
    const FieldHash::ConstIterator end = e.end();
    for( ; it != end; ++it)
    {
        const quint32 uds = it.key();
        s << uds;
        if (uds & KIO::UDSEntry::UDS_STRING)
            s << it->m_str;
        else if (uds & KIO::UDSEntry::UDS_NUMBER)
            s << it->m_long;
        else
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
     }
 }

void UDSEntryPrivate::load(QDataStream &s, UDSEntry &a)
{
    FieldHash &e = a.d->fields;

    e.clear();
    quint32 size;
    s >> size;

    // We cache the loaded strings. Some of them, like, e.g., the user,
    // will often be the same for many entries in a row. Caching them
    // permits to use implicit sharing to save memory.
    static QVector<QString> cachedStrings;
    if (cachedStrings.size() < size) {
        cachedStrings.resize(size);
    }

    for(quint32 i = 0; i < size; ++i)
    {
        quint32 uds;
        s >> uds;
        if (uds & KIO::UDSEntry::UDS_STRING) {
            // If the QString is the same like the one we read for the
            // previous UDSEntry at the i-th position, use an implicitly
            // shared copy of the same QString to save memory.
            QString buffer;
            s >> buffer;

            if (buffer != cachedStrings.at(i)) {
                 cachedStrings[i] = buffer;
            }

            Field f;
            f.m_str = cachedStrings.at(i);
            e.insert(uds, f);
        } else if (uds & KIO::UDSEntry::UDS_NUMBER) {
            Field f;
            s >> f.m_long;
            e.insert(uds, f);
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
}


