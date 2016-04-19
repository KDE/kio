/* This file is part of the KDE project
   Copyright (C) 2000-2005 David Faure <faure@kde.org>
   Copyright (C) 2007 Norbert Frese <nf2@scheinwelt.at>
   Copyright (C) 2007 Thiago Macieira <thiago@kde.org>
   Copyright (C) 2013-2014 Frank Reininghaus <frank78ac@googlemail.com>

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

#include <QString>
#include <QList>
#include <QDataStream>
#include <QVector>
#include <QDebug>

#include <KUser>

using namespace KIO;

/* ---------- UDSEntry ------------ */

class KIO::UDSEntryPrivate : public QSharedData
{
public:
    struct Field {
        inline Field(const QString &value) : m_str(value), m_long(0) {}
        inline Field(long long value = 0) : m_long(value) { }
        QString m_str;
        long long m_long;
    };

    QVector<Field> fields;

    // If udsIndexes[i] == uds, then fields[i] contains the value for 'uds'. Example:
    // udsIndexes = {UDS_NAME, UDS_FILE_SIZE, ...}
    // fields = {Field("filename"), Field(1234), ...}
    QVector<uint> udsIndexes;

    void insert(uint uds, const Field& field)
    {
        const int index = udsIndexes.indexOf(uds);
        if (index >= 0) {
            fields[index] = field;
        } else {
            udsIndexes.append(uds);
            fields.append(field);
        }
    }

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

// BUG: this API doesn't allow to handle symlinks correctly (we need buff from QT_LSTAT for most things, but buff from QT_STAT for st_mode and st_size)
UDSEntry::UDSEntry(const QT_STATBUF &buff, const QString &name)
    : d(new UDSEntryPrivate())
{
    reserve(9);
    insert(UDS_NAME,                name);
    insert(UDS_SIZE,                buff.st_size);
    insert(UDS_DEVICE_ID,           buff.st_dev);
    insert(UDS_INODE,               buff.st_ino);
    insert(UDS_FILE_TYPE,           buff.st_mode & QT_STAT_MASK); // extract file type
    insert(UDS_ACCESS,              buff.st_mode & 07777); // extract permissions
    insert(UDS_MODIFICATION_TIME,   buff.st_mtime);
    insert(UDS_ACCESS_TIME,         buff.st_atime);
#ifndef Q_OS_WIN
    insert(UDS_USER,                KUser(buff.st_uid).loginName());
    insert(UDS_GROUP,               KUserGroup(buff.st_gid).name());
#endif
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
    const int index = d->udsIndexes.indexOf(field);
    if (index >= 0) {
        return d->fields.at(index).m_str;
    } else {
        return QString();
    }
}

long long UDSEntry::numberValue(uint field, long long defaultValue) const
{
    const int index = d->udsIndexes.indexOf(field);
    if (index >= 0) {
        return d->fields.at(index).m_long;
    } else {
        return defaultValue;
    }
}

bool UDSEntry::isDir() const
{
    return (numberValue(UDS_FILE_TYPE) & QT_STAT_MASK) == QT_STAT_DIR;
}

bool UDSEntry::isLink() const
{
    return !stringValue(UDS_LINK_DEST).isEmpty();
}

void UDSEntry::reserve(int size)
{
    d->fields.reserve(size);
    d->udsIndexes.reserve(size);
}

void UDSEntry::insert(uint field, const QString &value)
{
    d->insert(field, UDSEntryPrivate::Field(value));
}

void UDSEntry::insert(uint field, long long value)
{
    d->insert(field, UDSEntryPrivate::Field(value));
}

#ifndef KIOCORE_NO_DEPRECATED
QList<uint> UDSEntry::listFields() const
{
    return d->udsIndexes.toList();
}
#endif

QVector<uint> UDSEntry::fields() const
{
    return d->udsIndexes;
}

int UDSEntry::count() const
{
    return d->udsIndexes.count();
}

bool UDSEntry::contains(uint field) const
{
    return d->udsIndexes.contains(field);
}

void UDSEntry::clear()
{
    d->fields.clear();
    d->udsIndexes.clear();
}

QDataStream &operator<<(QDataStream &s, const UDSEntry &a)
{
    UDSEntryPrivate::save(s, a);
    return s;
}

QDataStream &operator>>(QDataStream &s, UDSEntry &a)
{
    UDSEntryPrivate::load(s, a);
    return s;
}

void UDSEntryPrivate::save(QDataStream &s, const UDSEntry &a)
{
    const QVector<uint> &udsIndexes = a.d->udsIndexes;
    const QVector<Field> &fields = a.d->fields;
    const int size = udsIndexes.size();

    s << size;

    for (int index = 0; index < size; ++index) {
        uint uds = udsIndexes.at(index);
        s << uds;

        if (uds & KIO::UDSEntry::UDS_STRING) {
            s << fields.at(index).m_str;
        } else if (uds & KIO::UDSEntry::UDS_NUMBER) {
            s << fields.at(index).m_long;
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
}

KIOCORE_EXPORT QDebug operator<<(QDebug stream, const KIO::UDSEntry &entry)
{
    debugUDSEntry(stream, entry);
    return stream;
}

void UDSEntryPrivate::load(QDataStream &s, UDSEntry &a)
{
    a.clear();

    QVector<Field> &fields = a.d->fields;
    QVector<uint> &udsIndexes = a.d->udsIndexes;

    quint32 size;
    s >> size;
    fields.reserve(size);
    udsIndexes.reserve(size);

    // We cache the loaded strings. Some of them, like, e.g., the user,
    // will often be the same for many entries in a row. Caching them
    // permits to use implicit sharing to save memory.
    static QVector<QString> cachedStrings;
    if (quint32(cachedStrings.size()) < size) {
        cachedStrings.resize(size);
    }

    for (quint32 i = 0; i < size; ++i) {
        quint32 uds;
        s >> uds;
        udsIndexes.append(uds);

        if (uds & KIO::UDSEntry::UDS_STRING) {
            // If the QString is the same like the one we read for the
            // previous UDSEntry at the i-th position, use an implicitly
            // shared copy of the same QString to save memory.
            QString buffer;
            s >> buffer;

            if (buffer != cachedStrings.at(i)) {
                cachedStrings[i] = buffer;
            }

            fields.append(Field(cachedStrings.at(i)));
        } else if (uds & KIO::UDSEntry::UDS_NUMBER) {
            long long value;
            s >> value;
            fields.append(Field(value));
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
}

void debugUDSEntry(QDebug stream, const KIO::UDSEntry &entry)
{
    const QVector<uint> &udsIndexes = entry.d->udsIndexes;
    const QVector<KIO::UDSEntryPrivate::Field> &fields = entry.d->fields;
    const int size = udsIndexes.size();
    QDebugStateSaver saver(stream);
    stream.nospace() << "[";
    for (int index = 0; index < size; ++index) {
        const uint uds = udsIndexes.at(index);
        stream << " " << (uds & 0xffff) << "="; // we could use a switch statement for readability :-)
        if (uds & KIO::UDSEntry::UDS_STRING) {
            stream << fields.at(index).m_str;
        } else if (uds & KIO::UDSEntry::UDS_NUMBER) {
            stream << fields.at(index).m_long;
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
    stream << " ]";
}
