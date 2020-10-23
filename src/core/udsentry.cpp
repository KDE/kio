/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Norbert Frese <nf2@scheinwelt.at>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2013-2014 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "udsentry.h"

#include <QString>
#include <QDataStream>
#include <QVector>
#include <QDebug>

#include <KUser>

using namespace KIO;

//BEGIN UDSEntryPrivate
/* ---------- UDSEntryPrivate ------------ */

class KIO::UDSEntryPrivate : public QSharedData
{
public:
    void reserve(int size);
    void insert(uint udsField, const QString &value);
    void replace(uint udsField, const QString &value);
    void insert(uint udsField, long long value);
    void replace(uint udsField, long long value);
    int count() const;
    QString stringValue(uint udsField) const;
    long long numberValue(uint udsField, long long defaultValue = -1) const;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 8)
    QList<uint> listFields() const;
#endif
    QVector<uint> fields() const;
    bool contains(uint udsField) const;
    void clear();
    void save(QDataStream &s) const;
    void load(QDataStream &s);
    void debugUDSEntry(QDebug &stream) const;
    /**
     * @param field numeric UDS field id
     * @return the name of the field
     */
    static QString nameOfUdsField(uint field);

private:
    struct Field
    {
        inline Field() {}
        inline Field(const uint index, const QString &value) : m_str(value), m_index(index) {}
        inline Field(const uint index, long long value = 0) : m_long(value), m_index(index) {}

        QString m_str;
        long long m_long = LLONG_MIN;
        uint m_index = 0;
    };
    std::vector<Field> storage;
};

void UDSEntryPrivate::reserve(int size)
{
    storage.reserve(size);
}

void UDSEntryPrivate::insert(uint udsField, const QString &value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
    Q_ASSERT(std::find_if(storage.cbegin(), storage.cend(),
                                [udsField](const Field &entry) {return entry.m_index == udsField;}) == storage.cend());
    storage.emplace_back(udsField, value);
}

void UDSEntryPrivate::replace(uint udsField, const QString &value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
    auto it = std::find_if(storage.begin(), storage.end(),
                                [udsField](const Field &entry) {return entry.m_index == udsField;});
    if (it != storage.end()) {
        it->m_str = value;
        return;
    }
    storage.emplace_back(udsField, value);
}

void UDSEntryPrivate::insert(uint udsField, long long value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
    Q_ASSERT(std::find_if(storage.cbegin(), storage.cend(),
                                [udsField](const Field &entry) {return entry.m_index == udsField;}) == storage.cend());
    storage.emplace_back(udsField, value);
}

void UDSEntryPrivate::replace(uint udsField, long long value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
    auto it = std::find_if(storage.begin(), storage.end(),
                                [udsField](const Field &entry) {return entry.m_index == udsField;});
    if (it != storage.end()) {
        it->m_long = value;
        return;
    }
    storage.emplace_back(udsField, value);
}

int UDSEntryPrivate::count() const
{
    return storage.size();
}

QString UDSEntryPrivate::stringValue(uint udsField) const
{
    auto it = std::find_if(storage.cbegin(), storage.cend(),
                                [udsField](const Field &entry) {return entry.m_index == udsField;});
    if (it != storage.cend()) {
        return it->m_str;
    }
    return QString();
}

long long UDSEntryPrivate::numberValue(uint udsField, long long defaultValue) const
{
    auto it = std::find_if(storage.cbegin(), storage.cend(),
                                [udsField](const Field &entry) {return entry.m_index == udsField;});
    if (it != storage.cend()) {
        return it->m_long;
    }
    return defaultValue;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 8)
QList<uint> UDSEntryPrivate::listFields() const
{
    QList<uint> res;
    res.reserve(storage.size());
    for (const Field &field : storage) {
        res.append(field.m_index);
    }
    return res;
}
#endif

QVector<uint> UDSEntryPrivate::fields() const
{
    QVector<uint> res;
    res.reserve(storage.size());
    for (const Field &field : storage) {
        res.append(field.m_index);
    }
    return res;
}

bool UDSEntryPrivate::contains(uint udsField) const
{
    auto it = std::find_if(storage.cbegin(), storage.cend(),
                                [udsField](const Field &entry) {return entry.m_index == udsField;});
    return (it != storage.cend());
}

void UDSEntryPrivate::clear()
{
    storage.clear();
}

void UDSEntryPrivate::save(QDataStream &s) const
{
    s << static_cast<quint32>(storage.size());

    for (const Field &field : storage) {
        uint uds = field.m_index;
        s << uds;

        if (uds & KIO::UDSEntry::UDS_STRING) {
            s << field.m_str;
        } else if (uds & KIO::UDSEntry::UDS_NUMBER) {
            s << field.m_long;
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
}

void UDSEntryPrivate::load(QDataStream &s)
{
    clear();

    quint32 size;
    s >> size;
    reserve(size);

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

        if (uds & KIO::UDSEntry::UDS_STRING) {
            // If the QString is the same like the one we read for the
            // previous UDSEntry at the i-th position, use an implicitly
            // shared copy of the same QString to save memory.
            QString buffer;
            s >> buffer;

            if (buffer != cachedStrings.at(i)) {
                cachedStrings[i] = buffer;
            }

            insert(uds, cachedStrings.at(i));
        } else if (uds & KIO::UDSEntry::UDS_NUMBER) {
            long long value;
            s >> value;
            insert(uds, value);
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
}

QString UDSEntryPrivate::nameOfUdsField(uint field)
{
    switch (field) {
        case UDSEntry::UDS_SIZE:
            return QStringLiteral("UDS_SIZE");
        case UDSEntry::UDS_SIZE_LARGE:
            return QStringLiteral("UDS_SIZE_LARGE");
        case UDSEntry::UDS_USER:
            return QStringLiteral("UDS_USER");
        case UDSEntry::UDS_ICON_NAME:
            return QStringLiteral("UDS_ICON_NAME");
        case UDSEntry::UDS_GROUP:
            return QStringLiteral("UDS_GROUP");
        case UDSEntry::UDS_NAME:
            return QStringLiteral("UDS_NAME");
        case UDSEntry::UDS_LOCAL_PATH:
            return QStringLiteral("UDS_LOCAL_PATH");
        case UDSEntry::UDS_HIDDEN:
            return QStringLiteral("UDS_HIDDEN");
        case UDSEntry::UDS_ACCESS:
            return QStringLiteral("UDS_ACCESS");
        case UDSEntry::UDS_MODIFICATION_TIME:
            return QStringLiteral("UDS_MODIFICATION_TIME");
        case UDSEntry::UDS_ACCESS_TIME:
            return QStringLiteral("UDS_ACCESS_TIME");
        case UDSEntry::UDS_CREATION_TIME:
            return QStringLiteral("UDS_CREATION_TIME");
        case UDSEntry::UDS_FILE_TYPE:
            return QStringLiteral("UDS_FILE_TYPE");
        case UDSEntry::UDS_LINK_DEST:
            return QStringLiteral("UDS_LINK_DEST");
        case UDSEntry::UDS_URL:
            return QStringLiteral("UDS_URL");
        case UDSEntry::UDS_MIME_TYPE:
            return QStringLiteral("UDS_MIME_TYPE");
        case UDSEntry::UDS_GUESSED_MIME_TYPE:
            return QStringLiteral("UDS_GUESSED_MIME_TYPE");
        case UDSEntry::UDS_XML_PROPERTIES:
            return QStringLiteral("UDS_XML_PROPERTIES");
        case UDSEntry::UDS_EXTENDED_ACL:
            return QStringLiteral("UDS_EXTENDED_ACL");
        case UDSEntry::UDS_ACL_STRING:
            return QStringLiteral("UDS_ACL_STRING");
        case UDSEntry::UDS_DEFAULT_ACL_STRING:
            return QStringLiteral("UDS_DEFAULT_ACL_STRING");
        case UDSEntry::UDS_DISPLAY_NAME:
            return QStringLiteral("UDS_DISPLAY_NAME");
        case UDSEntry::UDS_TARGET_URL:
            return QStringLiteral("UDS_TARGET_URL");
        case UDSEntry::UDS_DISPLAY_TYPE:
            return QStringLiteral("UDS_DISPLAY_TYPE");
        case UDSEntry::UDS_ICON_OVERLAY_NAMES:
            return QStringLiteral("UDS_ICON_OVERLAY_NAMES");
        case UDSEntry::UDS_COMMENT:
            return QStringLiteral("UDS_COMMENT");
        case UDSEntry::UDS_DEVICE_ID:
            return QStringLiteral("UDS_DEVICE_ID");
        case UDSEntry::UDS_INODE:
            return QStringLiteral("UDS_INODE");
        case UDSEntry::UDS_EXTRA:
            return QStringLiteral("UDS_EXTRA");
        case UDSEntry::UDS_EXTRA_END:
            return QStringLiteral("UDS_EXTRA_END");
        default:
            return QStringLiteral("Unknown uds field %1").arg(field);
    }
}

void UDSEntryPrivate::debugUDSEntry(QDebug &stream) const
{
    QDebugStateSaver saver(stream);
    stream.nospace() << "[";
    for (const Field &field : storage) {
        stream << " " << nameOfUdsField(field.m_index) << "=";
        if (field.m_index & KIO::UDSEntry::UDS_STRING) {
            stream << field.m_str;
        } else if (field.m_index & KIO::UDSEntry::UDS_NUMBER) {
            stream << field.m_long;
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
    stream << " ]";
}
//END UDSEntryPrivate

//BEGIN UDSEntry
/* ---------- UDSEntry ------------ */

UDSEntry::UDSEntry()
    : d(new UDSEntryPrivate())
{
}

// BUG: this API doesn't allow to handle symlinks correctly (we need buff from QT_LSTAT for most things, but buff from QT_STAT for st_mode and st_size)
UDSEntry::UDSEntry(const QT_STATBUF &buff, const QString &name)
    : d(new UDSEntryPrivate())
{
#ifndef Q_OS_WIN
    d->reserve(10);
#else
    d->reserve(8);
#endif
    d->insert(UDS_NAME,                name);
    d->insert(UDS_SIZE,                buff.st_size);
    d->insert(UDS_DEVICE_ID,           buff.st_dev);
    d->insert(UDS_INODE,               buff.st_ino);
    d->insert(UDS_FILE_TYPE,           buff.st_mode & QT_STAT_MASK); // extract file type
    d->insert(UDS_ACCESS,              buff.st_mode & 07777); // extract permissions
    d->insert(UDS_MODIFICATION_TIME,   buff.st_mtime);
    d->insert(UDS_ACCESS_TIME,         buff.st_atime);
#ifndef Q_OS_WIN
    d->insert(UDS_USER,                KUser(buff.st_uid).loginName());
    d->insert(UDS_GROUP,               KUserGroup(buff.st_gid).name());
#endif
}

UDSEntry::UDSEntry(const UDSEntry&) = default;
UDSEntry::~UDSEntry() = default;
UDSEntry::UDSEntry(UDSEntry&&) = default;
UDSEntry& UDSEntry::operator=(const UDSEntry&) = default;
UDSEntry& UDSEntry::operator=(UDSEntry&&) = default;

QString UDSEntry::stringValue(uint field) const
{
    return d->stringValue(field);
}

long long UDSEntry::numberValue(uint field, long long defaultValue) const
{
    return d->numberValue(field, defaultValue);
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
    d->reserve(size);
}

void UDSEntry::fastInsert(uint field, const QString &value)
{
    d->insert(field, value);
}

void UDSEntry::fastInsert(uint field, long long value)
{
    d->insert(field, value);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 48)
void UDSEntry::insert(uint field, const QString &value)
{
    d->replace(field, value);
}
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 48)
void UDSEntry::insert(uint field, long long value)
{
    d->replace(field, value);
}
#endif

void UDSEntry::replace(uint field, const QString &value)
{
    d->replace(field, value);
}

void UDSEntry::replace(uint field, long long value)
{
    d->replace(field, value);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 8)
QList<uint> UDSEntry::listFields() const
{
    return d->listFields();
}
#endif

QVector<uint> UDSEntry::fields() const
{
    return d->fields();
}

int UDSEntry::count() const
{
    return d->count();
}

bool UDSEntry::contains(uint field) const
{
    return d->contains(field);
}

void UDSEntry::clear()
{
    d->clear();
}
//END UDSEntry

KIOCORE_EXPORT QDebug operator<<(QDebug stream, const KIO::UDSEntry &entry)
{
    entry.d->debugUDSEntry(stream);
    return stream;
}

KIOCORE_EXPORT QDataStream &operator<<(QDataStream &s, const KIO::UDSEntry &a)
{
    a.d->save(s);
    return s;
}

KIOCORE_EXPORT QDataStream &operator>>(QDataStream &s, KIO::UDSEntry &a)
{
    a.d->load(s);
    return s;
}

KIOCORE_EXPORT bool operator==(const KIO::UDSEntry &entry, const KIO::UDSEntry &other)
{
    if (entry.count() != other.count()) {
         return false;
     }

     const QVector<uint> fields = entry.fields();
     for (uint field : fields) {
         if (!other.contains(field)) {
             return false;
         }

         if (field & UDSEntry::UDS_STRING) {
             if (entry.stringValue(field) != other.stringValue(field)) {
                 return false;
             }
         } else {
             if (entry.numberValue(field) != other.numberValue(field)) {
                 return false;
             }
         }
     }

     return true;
}

KIOCORE_EXPORT bool operator!=(const KIO::UDSEntry &entry, const KIO::UDSEntry &other)
{
    return !(entry == other);
}
