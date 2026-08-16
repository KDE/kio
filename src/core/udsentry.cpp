/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Norbert Frese <nf2@scheinwelt.at>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2013-2014 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "udsentry.h"

#include "../kioworkers/file/stat_unix.h"
#include "../utils_p.h"

#include <QDataStream>
#include <QDebug>
#include <QString>

#include <algorithm>

#include <KUser>

using namespace KIO;

// BEGIN UDSEntryPrivate

namespace
{
// The fields an entry carries, in the order they were given. A listing hands the same fields over
// from one file to the next, so a sequence is made once and every entry carrying those fields points
// at it, which leaves an entry holding its values alone.
struct FieldSequence {
    std::vector<uint> stringFields;
    std::vector<uint> numberFields;
    // The sequences carrying these fields and one more, grown by the thread which made this one.
    std::vector<std::pair<uint, const FieldSequence *>> longer;
    const void *owner = nullptr;
};

// The sequences a thread has met. They are kept for as long as the program runs, which is what lets
// an entry built on one thread be read on another. There is one for each shape an entry takes, a
// handful in practice, so a thread that meets a sequence made elsewhere looks its own up rather than
// writing to something another thread reads.
struct FieldSequenceStore {
    std::vector<const FieldSequence *> known;
    FieldSequence *empty = nullptr;
};

FieldSequenceStore &sequenceStore()
{
    static thread_local FieldSequenceStore store;
    if (!store.empty) {
        store.empty = new FieldSequence;
        store.empty->owner = &store;
        store.known.push_back(store.empty);
    }
    return store;
}

const FieldSequence *emptySequence()
{
    return sequenceStore().empty;
}

const FieldSequence *sequenceFor(const std::vector<uint> &stringFields, const std::vector<uint> &numberFields)
{
    FieldSequenceStore &store = sequenceStore();
    // The shape of an entry repeats, so the one met last is looked at first.
    for (auto it = store.known.rbegin(); it != store.known.rend(); ++it) {
        if ((*it)->stringFields == stringFields && (*it)->numberFields == numberFields) {
            return *it;
        }
    }
    FieldSequence *sequence = new FieldSequence{stringFields, numberFields, {}, &store};
    store.known.push_back(sequence);
    return sequence;
}

// The sequence carrying the fields of \a from with \a field after them.
const FieldSequence *sequenceWith(const FieldSequence *from, uint field, bool isString)
{
    FieldSequenceStore &store = sequenceStore();
    const bool ours = from->owner == &store;
    if (ours) {
        for (const auto &[knownField, longer] : from->longer) {
            if (knownField == field) {
                return longer;
            }
        }
    }

    std::vector<uint> stringFields = from->stringFields;
    std::vector<uint> numberFields = from->numberFields;
    (isString ? stringFields : numberFields).push_back(field);
    const FieldSequence *longer = sequenceFor(stringFields, numberFields);
    if (ours) {
        const_cast<FieldSequence *>(from)->longer.emplace_back(field, longer);
    }
    return longer;
}
}

class KIO::UDSEntryPrivate : public QSharedData
{
public:
    void reserveNumbers(int size);
    void reserveStrings(int size);
    void reserve(std::initializer_list<uint> fields);
    void insert(std::initializer_list<std::pair<uint, long long>> fields);
    void insert(uint udsField, const QString &value);
    void replace(uint udsField, const QString &value);
    void insert(std::initializer_list<std::pair<uint, const QString &>> fields);
    void insert(uint udsField, long long value);
    void replace(uint udsField, long long value);
    int count() const;
    int numbersCount() const;
    int stringsCount() const;
    QString stringValue(uint udsField) const;
    long long numberValue(uint udsField, long long defaultValue = -1) const;
    QList<uint> fields() const;
    bool contains(uint udsField) const;
    void clear();
    void save(QDataStream &s) const;
    void load(QDataStream &s);
    void debugUDSEntry(QDebug &stream) const;
    /*
     * \a field numeric UDS field id
     * Returns the name of the field
     */
    static QString nameOfUdsField(uint field);

    UDSEntryPrivate() = default;
    UDSEntryPrivate(const UDSEntryPrivate &other);
    UDSEntryPrivate &operator=(const UDSEntryPrivate &) = delete;
    ~UDSEntryPrivate();

private:
    // The values of an entry lie in one block, the strings first and the numbers after them, so that
    // an entry asks the allocator once. Which field each value belongs to is read off the sequence.
    QString *stringSlot(size_t index) const
    {
        return static_cast<QString *>(valueBlock) + index;
    }

    long long *numberSlot(size_t index) const
    {
        return reinterpret_cast<long long *>(static_cast<QString *>(valueBlock) + stringCapacity) + index;
    }

    size_t stringCount() const
    {
        return fieldSequence->stringFields.size();
    }

    size_t numberCount() const
    {
        return fieldSequence->numberFields.size();
    }

    // Moves the values into a block with room for the given numbers of them.
    void reallocate(size_t strings, size_t numbers);

    void makeRoom(size_t moreStrings, size_t moreNumbers)
    {
        const size_t strings = stringCount() + moreStrings;
        const size_t numbers = numberCount() + moreNumbers;
        if (strings <= stringCapacity && numbers <= numberCapacity) {
            return;
        }

        // A caller which says how many fields are coming, by reserving or by handing them over
        // together, gets room for exactly those. One which inserts a field at a time gets room that
        // doubles, so that the values are moved a few times rather than at every field.
        const size_t stringRoom = strings > stringCapacity ? qMax(strings, size_t(stringCapacity) * 2) : stringCapacity;
        const size_t numberRoom = numbers > numberCapacity ? qMax(numbers, size_t(numberCapacity) * 2) : numberCapacity;
        reallocate(stringRoom, numberRoom);
    }

    void release();

    // The fields this entry carries, shared with every entry carrying the same ones.
    const FieldSequence *fieldSequence = emptySequence();
    void *valueBlock = nullptr;
    quint32 stringCapacity = 0;
    quint32 numberCapacity = 0;
};

UDSEntryPrivate::UDSEntryPrivate(const UDSEntryPrivate &other)
    : QSharedData(other)
{
    const size_t strings = other.stringCount();
    const size_t numbers = other.numberCount();
    // The block is made while this entry still carries no field, so nothing is moved into it, and
    // the sequence is taken on once the values are in place.
    if (strings > 0 || numbers > 0) {
        reallocate(strings, numbers);
        for (size_t i = 0; i < strings; ++i) {
            new (stringSlot(i)) QString(*other.stringSlot(i));
        }
        for (size_t i = 0; i < numbers; ++i) {
            *numberSlot(i) = *other.numberSlot(i);
        }
    }
    fieldSequence = other.fieldSequence;
}

UDSEntryPrivate::~UDSEntryPrivate()
{
    release();
}

void UDSEntryPrivate::release()
{
    if (!valueBlock) {
        return;
    }
    for (size_t i = 0; i < stringCount(); ++i) {
        stringSlot(i)->~QString();
    }
    ::operator delete(valueBlock);
    valueBlock = nullptr;
    stringCapacity = 0;
    numberCapacity = 0;
}

void UDSEntryPrivate::reallocate(size_t strings, size_t numbers)
{
    void *block = ::operator new(strings * sizeof(QString) + numbers * sizeof(long long));
    QString *newStrings = static_cast<QString *>(block);
    long long *newNumbers = reinterpret_cast<long long *>(newStrings + strings);

    for (size_t i = 0; i < stringCount(); ++i) {
        new (newStrings + i) QString(std::move(*stringSlot(i)));
        stringSlot(i)->~QString();
    }
    for (size_t i = 0; i < numberCount(); ++i) {
        newNumbers[i] = *numberSlot(i);
    }

    if (valueBlock) {
        ::operator delete(valueBlock);
    }
    valueBlock = block;
    stringCapacity = quint32(strings);
    numberCapacity = quint32(numbers);
}

void UDSEntryPrivate::reserve(std::initializer_list<uint> fields)
{
    int stringSize = 0;
    int numberSize = 0;
    for (const auto f : fields) {
        if (f & UDSEntry::UDS_NUMBER) {
            numberSize += 1;
        } else {
            stringSize += 1;
        }
    }
    reserveStrings(stringSize);
    reserveNumbers(numberSize);
}

void UDSEntryPrivate::reserveStrings(int size)
{
    if (size_t(size) > stringCapacity) {
        reallocate(size, numberCapacity);
    }
}

void UDSEntryPrivate::reserveNumbers(int size)
{
    if (size_t(size) > numberCapacity) {
        reallocate(stringCapacity, size);
    }
}

void UDSEntryPrivate::insert(std::initializer_list<std::pair<uint, const QString &>> fieldValuePairs)
{
    // The block is sized for the whole batch, so it is made once rather than at every field.
    makeRoom(fieldValuePairs.size(), 0);
    for (const auto &f : fieldValuePairs) {
        insert(f.first, f.second);
    }
}

void UDSEntryPrivate::insert(uint udsField, const QString &value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
    Q_ASSERT(std::ranges::find(fieldSequence->stringFields, udsField) == fieldSequence->stringFields.cend());
    makeRoom(1, 0);
    new (stringSlot(stringCount())) QString(value);
    fieldSequence = sequenceWith(fieldSequence, udsField, true);
}

void UDSEntryPrivate::replace(uint udsField, const QString &value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
    const std::vector<uint> &fields = fieldSequence->stringFields;
    const auto it = std::ranges::find(fields, udsField);
    if (it != fields.cend()) {
        *stringSlot(it - fields.cbegin()) = value;
        return;
    }
    insert(udsField, value);
}

void UDSEntryPrivate::insert(std::initializer_list<std::pair<uint, long long>> fieldValuePairs)
{
    // The block is sized for the whole batch, so it is made once rather than at every field.
    makeRoom(0, fieldValuePairs.size());
    for (const auto &f : fieldValuePairs) {
        insert(f.first, f.second);
    }
}

void UDSEntryPrivate::insert(uint udsField, long long value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
    Q_ASSERT(std::ranges::find(fieldSequence->numberFields, udsField) == fieldSequence->numberFields.cend());
    makeRoom(0, 1);
    *numberSlot(numberCount()) = value;
    fieldSequence = sequenceWith(fieldSequence, udsField, false);
}

void UDSEntryPrivate::replace(uint udsField, long long value)
{
    Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
    const std::vector<uint> &fields = fieldSequence->numberFields;
    const auto it = std::ranges::find(fields, udsField);
    if (it != fields.cend()) {
        *numberSlot(it - fields.cbegin()) = value;
        return;
    }
    insert(udsField, value);
}

int UDSEntryPrivate::count() const
{
    return int(stringCount() + numberCount());
}
int UDSEntryPrivate::numbersCount() const
{
    return int(numberCount());
}
int UDSEntryPrivate::stringsCount() const
{
    return int(stringCount());
}

QString UDSEntryPrivate::stringValue(uint udsField) const
{
    const std::vector<uint> &fields = fieldSequence->stringFields;
    const auto it = std::ranges::find(fields, udsField);
    if (it != fields.cend()) {
        return *stringSlot(it - fields.cbegin());
    }
    return QString();
}

long long UDSEntryPrivate::numberValue(uint udsField, long long defaultValue) const
{
    const std::vector<uint> &fields = fieldSequence->numberFields;
    const auto it = std::ranges::find(fields, udsField);
    if (it != fields.cend()) {
        return *numberSlot(it - fields.cbegin());
    }
    return defaultValue;
}

QList<uint> UDSEntryPrivate::fields() const
{
    QList<uint> res;
    res.reserve(stringCount() + numberCount());
    for (uint field : fieldSequence->stringFields) {
        res.append(field);
    }
    for (uint field : fieldSequence->numberFields) {
        res.append(field);
    }
    return res;
}

bool UDSEntryPrivate::contains(uint udsField) const
{
    const std::vector<uint> &fields = (udsField & KIO::UDSEntry::UDS_NUMBER) ? fieldSequence->numberFields : fieldSequence->stringFields;
    return std::ranges::find(fields, udsField) != fields.cend();
}

void UDSEntryPrivate::clear()
{
    release();
    fieldSequence = emptySequence();
}

void UDSEntryPrivate::save(QDataStream &s) const
{
    s << static_cast<quint32>(stringCount() + numberCount());

    for (size_t i = 0; i < stringCount(); ++i) {
        const uint uds = fieldSequence->stringFields[i];
        s << uds;

        if (uds & KIO::UDSEntry::UDS_STRING) [[likely]] {
            s << *stringSlot(i);
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }

    for (size_t i = 0; i < numberCount(); ++i) {
        const uint uds = fieldSequence->numberFields[i];
        s << uds;

        if (uds & KIO::UDSEntry::UDS_NUMBER) [[likely]] {
            s << *numberSlot(i);
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an invalid type");
        }
    }
}

// The value of these fields names the item, so no two entries of a listing carry the same one. The
// target of a link and the name an item is displayed under can be the same for several items.
static bool namesTheItem(uint udsField)
{
    switch (udsField) {
    case UDSEntry::UDS_NAME:
    case UDSEntry::UDS_URL:
    case UDSEntry::UDS_LOCAL_PATH:
        return true;
    default:
        return false;
    }
}

void UDSEntryPrivate::load(QDataStream &s)
{
    clear();

    quint32 size;
    s >> size;
    // Buffers that live as long as the thread, so the fields of an entry are known before its
    // sequence is looked up and each of its arrays takes exactly what it holds.
    thread_local std::vector<uint> stagedStringFields;
    thread_local std::vector<uint> stagedNumberFields;
    thread_local std::vector<QString> stagedStrings;
    thread_local std::vector<long long> stagedNumbers;
    stagedStringFields.clear();
    stagedNumberFields.clear();
    stagedStrings.clear();
    stagedNumbers.clear();

    // We cache the loaded strings. Some of them, like, e.g., the user,
    // will often be the same for many entries in a row. Caching them
    // permits to use implicit sharing to save memory.
    thread_local QList<QString> cachedStrings;
    if (quint32(cachedStrings.size()) < size) {
        cachedStrings.resize(size);
    }

    // We cache the string buffer instance, increasing the capacity by usage,
    // only change the size on it.
    // Relying on (undocumented/unspecified) behaviour of
    // operator>>(QDataStream &stream, QString &string)
    // to reuse existing allocation.
    thread_local QString buffer;

    // The entries of a listing carry the same fields in the same order, so the sequence of the entry
    // before this one is followed while the fields match it, and the ids are written down only from
    // the first field that does not.
    thread_local const FieldSequence *lastSequence = nullptr;
    const FieldSequence *expected = lastSequence;
    bool asExpected = expected != nullptr;
    size_t stringCount = 0;
    size_t numberCount = 0;

    auto writeDownFieldsSoFar = [&]() {
        stagedStringFields.assign(expected->stringFields.cbegin(), expected->stringFields.cbegin() + stringCount);
        stagedNumberFields.assign(expected->numberFields.cbegin(), expected->numberFields.cbegin() + numberCount);
    };

    for (quint32 i = 0; i < size; ++i) {
        quint32 uds;
        s >> uds;

        if (uds & KIO::UDSEntry::UDS_STRING) {
            s >> buffer;

            if (asExpected) {
                if (stringCount < expected->stringFields.size() && expected->stringFields[stringCount] == uds) {
                    ++stringCount;
                } else {
                    asExpected = false;
                    writeDownFieldsSoFar();
                    stagedStringFields.push_back(uds);
                }
            } else {
                stagedStringFields.push_back(uds);
            }

            if (namesTheItem(uds)) {
                stagedStrings.push_back(buffer);
            } else {
                // Values repeat from one entry to the next often enough that sharing one is worth
                // a comparison.
                QString &cachedString = cachedStrings[i];
                if (buffer != cachedString) {
                    cachedString = buffer;
                }

                stagedStrings.push_back(cachedString);
            }
        } else if (uds & KIO::UDSEntry::UDS_NUMBER) {
            long long value;
            s >> value;

            if (asExpected) {
                if (numberCount < expected->numberFields.size() && expected->numberFields[numberCount] == uds) {
                    ++numberCount;
                } else {
                    asExpected = false;
                    writeDownFieldsSoFar();
                    stagedNumberFields.push_back(uds);
                }
            } else {
                stagedNumberFields.push_back(uds);
            }

            stagedNumbers.push_back(value);
        } else {
            Q_ASSERT_X(false, "KIO::UDSEntry", "Found a field with an unexpected type");
        }
    }

    // The fields are known, so the block takes exactly what the entry holds. It is made while this
    // entry still carries no field, so nothing is moved into it.
    reallocate(stagedStrings.size(), stagedNumbers.size());
    for (size_t i = 0; i < stagedStrings.size(); ++i) {
        new (stringSlot(i)) QString(stagedStrings[i]);
    }
    for (size_t i = 0; i < stagedNumbers.size(); ++i) {
        *numberSlot(i) = stagedNumbers[i];
    }

    if (asExpected && stringCount == expected->stringFields.size() && numberCount == expected->numberFields.size()) {
        fieldSequence = expected;
    } else {
        if (asExpected) {
            // Every field matched but the entry carries fewer of them than the one before it.
            writeDownFieldsSoFar();
        }
        fieldSequence = sequenceFor(stagedStringFields, stagedNumberFields);
    }
    lastSequence = fieldSequence;
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
    case UDSEntry::UDS_LOCAL_GROUP_ID:
        return QStringLiteral("UDS_LOCAL_GROUP_ID");
    case UDSEntry::UDS_LOCAL_USER_ID:
        return QStringLiteral("UDS_LOCAL_USER_ID");
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
    case UDSEntry::UDS_SUBVOL_ID:
        return QStringLiteral("UDS_SUBVOL_ID");
    case UDSEntry::UDS_MOUNT_ID:
        return QStringLiteral("UDS_MOUNT_ID");
    case UDSEntry::UDS_MODIFICATION_TIME_NS_OFFSET:
        return QStringLiteral("UDS_MODIFICATION_TIME_NS_OFFSET");
    case UDSEntry::UDS_ACCESS_TIME_NS_OFFSET:
        return QStringLiteral("UDS_ACCESS_TIME_NS_OFFSET");
    case UDSEntry::UDS_CREATION_TIME_NS_OFFSET:
        return QStringLiteral("UDS_CREATION_TIME_NS_OFFSET");
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
    for (size_t i = 0; i < stringCount(); ++i) {
        stream << " " << nameOfUdsField(fieldSequence->stringFields[i]) << "=" << *stringSlot(i);
    }
    for (size_t i = 0; i < numberCount(); ++i) {
        stream << " " << nameOfUdsField(fieldSequence->numberFields[i]) << "=" << *numberSlot(i);
    }
    stream << " ]";
}
// END UDSEntryPrivate

// BEGIN UDSEntry
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
    d->reserveNumbers(11);
#else
    d->reserveNumbers(9);
#endif
    d->reserveStrings(1);
    d->insert(UDS_NAME, name);
    d->insert(UDS_SIZE, buff.st_size);
    d->insert(UDS_DEVICE_ID, buff.st_dev);
    d->insert(UDS_INODE, buff.st_ino);
    d->insert(UDS_FILE_TYPE, buff.st_mode & QT_STAT_MASK); // extract file type
    d->insert(UDS_ACCESS, buff.st_mode & 07777); // extract permissions
    d->insert(UDS_MODIFICATION_TIME, stat_mtime(buff));
    d->insert(UDS_MODIFICATION_TIME_NS_OFFSET, stat_mtime_ns(buff));
    d->insert(UDS_ACCESS_TIME, stat_atime(buff));
    d->insert(UDS_ACCESS_TIME_NS_OFFSET, stat_atime_ns(buff));
#if !defined(Q_OS_WIN)
    d->insert(UDS_LOCAL_USER_ID, buff.st_uid);
    d->insert(UDS_LOCAL_GROUP_ID, buff.st_gid);
#endif
}

UDSEntry::UDSEntry(const UDSEntry &) = default;
UDSEntry::~UDSEntry() = default;
UDSEntry::UDSEntry(UDSEntry &&) = default;
UDSEntry &UDSEntry::operator=(const UDSEntry &) = default;
UDSEntry &UDSEntry::operator=(UDSEntry &&) = default;

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
    return Utils::isDirMask(numberValue(UDS_FILE_TYPE));
}

bool UDSEntry::isLink() const
{
    return !stringValue(UDS_LINK_DEST).isEmpty();
}

void KIO::UDSEntry::reserveStrings(int size)
{
    d->reserveStrings(size);
}

void KIO::UDSEntry::reserveNumbers(int size)
{
    d->reserveNumbers(size);
}

void UDSEntry::fastInsert(uint field, const QString &value)
{
    d->insert(field, value);
}

void UDSEntry::fastInsert(uint field, long long value)
{
    d->insert(field, value);
}

void UDSEntry::replace(uint field, const QString &value)
{
    d->replace(field, value);
}

void UDSEntry::replace(uint field, long long value)
{
    d->replace(field, value);
}

QList<uint> UDSEntry::fields() const
{
    return d->fields();
}

int UDSEntry::count() const
{
    return d->count();
}

int KIO::UDSEntry::stringsCount() const
{
    return d->stringsCount();
}

int KIO::UDSEntry::numbersCount() const
{
    return d->numbersCount();
}

bool UDSEntry::contains(uint field) const
{
    return d->contains(field);
}

void UDSEntry::clear()
{
    d->clear();
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(6, 29)
void UDSEntry::reserve(int size)
{
    d->reserveStrings(size / 3);
    d->reserveNumbers(size * 2 / 3);
}
#endif

void UDSEntry::reserve(std::initializer_list<uint> fields)
{
    d->reserve(fields);
}

void UDSEntry::insert(std::initializer_list<std::pair<uint, const QString &>> fieldValuePairs)
{
    d->insert(fieldValuePairs);
}

void UDSEntry::insert(std::initializer_list<std::pair<uint, long long>> fieldValuePairs)
{
    d->insert(fieldValuePairs);
}
// END UDSEntry

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
    // Dereferencing the d pointer directly would cause a detach because a is not const. This is entirely moot
    // because we are going to overwrite it anyway, so create a new one and swap it instead.
    auto newD = QSharedDataPointer<UDSEntryPrivate>(new UDSEntryPrivate);
    newD->load(s);
    a.d.swap(newD);
    return s;
}

// TODO KF7 remove
// legacy operator in global namespace for binary compatibility
KIOCORE_EXPORT bool operator==(const KIO::UDSEntry &entry, const KIO::UDSEntry &other)
{
    return KIO::operator==(entry, other);
}

// TODO KF7 remove
// legacy operator in global namespace for binary compatibility
KIOCORE_EXPORT bool operator!=(const KIO::UDSEntry &entry, const KIO::UDSEntry &other)
{
    return KIO::operator!=(entry, other);
}

bool KIO::operator==(const KIO::UDSEntry &entry, const KIO::UDSEntry &other)
{
    if (entry.count() != other.count()) {
        return false;
    }

    const QList<uint> fields = entry.fields();
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

bool KIO::operator!=(const KIO::UDSEntry &entry, const KIO::UDSEntry &other)
{
    return !KIO::operator==(entry, other);
}
