/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2014 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2024 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <QHash>
#include <QList>

#include <QElapsedTimer>
#include <QFile>

#include <memory>
#include <vector>

#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include <kio/global.h> // filesize_t
#include <kio/udsentry.h>

/*
   This is to compare the old list-of-lists API vs a QMap/QHash-based API
   in terms of performance.

   The number of atoms and their type map to what kio_file would put in
   for any normal file.

   The lookups are done for two atoms that are present, and for one that is not.
*/

class UdsEntryBenchmark : public QObject
{
    Q_OBJECT
public:
    UdsEntryBenchmark()
        : nameStr(QStringLiteral("name"))
        , groupStr(QStringLiteral("group"))
        , now(QDateTime::currentDateTime())
        , now_time_t(now.toSecsSinceEpoch())
    {
    }
private Q_SLOTS:

    void testAnotherFill();
    void testTwoVectorKindEntryFill();
    void testAnotherV2Fill();
    void testTwoVectorsFill();
    void testUDSEntryHSFill();
    void testUnionFill();
    void testInternedFieldsFill();
    void testInternedSingleBlockFill();

    void testAnotherCompare();
    void testTwoVectorKindEntryCompare();
    void testAnotherV2Compare();
    void testTwoVectorsCompare();
    void testUDSEntryHSCompare();
    void testUnionCompare();
    void testInternedFieldsCompare();
    void testInternedSingleBlockCompare();

    void testAnotherApp();
    void testTwoVectorKindEntryApp();
    void testAnotherV2App();
    void testTwoVectorsApp();
    void testUDSEntryHSApp();
    void testUnionApp();
    void testInternedFieldsApp();
    void testInternedSingleBlockApp();

    void testspaceUsed();
    void testDolphinLikeWorkload();

public:
    const QString nameStr;
    const QString groupStr;
    const QDateTime now;
    const time_t now_time_t;
};

// The KDE4 solution: QHash+struct

// Which one is used depends on UDS_STRING vs UDS_LONG
struct UDSAtom4 { // can't be a union due to qstring...
    UDSAtom4()
    {
    } // for QHash or QMap
    UDSAtom4(const QString &s)
        : m_str(s)
    {
    }
    UDSAtom4(long long l)
        : m_long(l)
    {
    }

    QString m_str;
    long long m_long;
};

// Another possibility, to save on QVariant costs
// hash+struct
// using QMap is slower
class UDSEntryHS : public QHash<uint, UDSAtom4>
{
public:
    void replaceOrInsert(uint udsField, const QString &value)
    {
        insert(udsField, value);
    }
    void replaceOrInsert(uint udsField, long long value)
    {
        insert(udsField, value);
    }
    int count() const
    {
        return size();
    }
    QString stringValue(uint udsField) const
    {
        return value(udsField).m_str;
    }
    long long numberValue(uint udsField, long long defaultValue = -1) const
    {
        if (contains(udsField)) {
            return value(udsField).m_long;
        }
        return defaultValue;
    }
    QString spaceUsed()
    {
        return QStringLiteral("size:%1 space used:%2")
            .arg(size() * sizeof(UDSAtom4) + sizeof(QHash<uint, UDSAtom4>))
            .arg(capacity() * sizeof(UDSAtom4) + sizeof(QHash<uint, UDSAtom4>));
    }
};

// Frank's suggestion in https://git.reviewboard.kde.org/r/118452/
class FrankUDSEntry
{
public:
    class Field
    {
    public:
        inline Field(const QString &value)
            : m_str(value)
            , m_long(0)
        {
        }
        inline Field(long long value = 0)
            : m_long(value)
        {
        }
        QString m_str;
        long long m_long;
    };
    QList<Field> fields;
    // If udsIndexes[i] == uds, then fields[i] contains the value for 'uds'.
    QList<uint> udsIndexes;

    void reserve(int size)
    {
        fields.reserve(size);
        udsIndexes.reserve(size);
    }
    void insert(uint udsField, const QString &value)
    {
        const int index = udsIndexes.indexOf(udsField);
        if (index >= 0) {
            fields[index] = Field(value);
        } else {
            udsIndexes.append(udsField);
            fields.append(Field(value));
        }
    }
    void replaceOrInsert(uint udsField, const QString &value)
    {
        insert(udsField, value);
    }
    void insert(uint udsField, long long value)
    {
        const int index = udsIndexes.indexOf(udsField);
        if (index >= 0) {
            fields[index] = Field(value);
        } else {
            udsIndexes.append(udsField);
            fields.append(Field(value));
        }
    }
    void replaceOrInsert(uint udsField, long long value)
    {
        insert(udsField, value);
    }
    int count() const
    {
        return udsIndexes.count();
    }
    QString stringValue(uint udsField) const
    {
        const int index = udsIndexes.indexOf(udsField);
        if (index >= 0) {
            return fields.at(index).m_str;
        } else {
            return QString();
        }
    }
    long long numberValue(uint udsField, long long defaultValue = -1) const
    {
        const int index = udsIndexes.indexOf(udsField);
        if (index >= 0) {
            return fields.at(index).m_long;
        } else {
            return defaultValue;
        }
    }
    QString spaceUsed()
    {
        return QStringLiteral("size:%1 space used:%2")
            .arg(fields.size() * sizeof(Field) + udsIndexes.size() * sizeof(uint) + sizeof(std::vector<Field>) + sizeof(std::vector<uint>))
            .arg(fields.capacity() * sizeof(Field) + udsIndexes.capacity() * sizeof(uint) + sizeof(std::vector<Field>) + sizeof(std::vector<uint>));
    }
};

// Instead of two vectors, use only one
// KF 5
class AnotherUDSEntry
{
private:
    struct Field {
        inline Field() noexcept
        {
        }
        inline Field(const uint index, const QString &value) noexcept
            : m_str(value)
            , m_index(index)
        {
        }
        inline Field(const uint index, long long value = 0) noexcept
            : m_long(value)
            , m_index(index)
        {
        }
        // This operator helps to gain 1ms just comparing the key
        inline bool operator==(const Field &other) const
        {
            return m_index == other.m_index;
        }

        QString m_str;
        long long m_long = LLONG_MIN;
        uint m_index = 0;
    };
    std::vector<Field> storage;

public:
    void reserve(int size)
    {
        storage.reserve(size);
    }
    void insert(uint udsField, const QString &value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
        Q_ASSERT(std::find_if(storage.cbegin(),
                              storage.cend(),
                              [udsField](const Field &entry) {
                                  return entry.m_index == udsField;
                              })
                 == storage.cend());
        storage.emplace_back(udsField, value);
    }
    void replaceOrInsert(uint udsField, const QString &value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
        auto it = std::find_if(storage.begin(), storage.end(), [udsField](const Field &entry) {
            return entry.m_index == udsField;
        });
        if (it != storage.end()) {
            it->m_str = value;
            return;
        }
        storage.emplace_back(udsField, value);
    }
    void insert(uint udsField, long long value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
        Q_ASSERT(std::find_if(storage.cbegin(),
                              storage.cend(),
                              [udsField](const Field &entry) {
                                  return entry.m_index == udsField;
                              })
                 == storage.cend());
        storage.emplace_back(udsField, value);
    }
    void replaceOrInsert(uint udsField, long long value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
        auto it = std::find_if(storage.begin(), storage.end(), [udsField](const Field &entry) {
            return entry.m_index == udsField;
        });
        if (it != storage.end()) {
            it->m_long = value;
            return;
        }
        storage.emplace_back(udsField, value);
    }
    int count() const
    {
        return storage.size();
    }
    QString stringValue(uint udsField) const
    {
        auto it = std::find_if(storage.cbegin(), storage.cend(), [udsField](const Field &entry) {
            return entry.m_index == udsField;
        });
        if (it != storage.cend()) {
            return it->m_str;
        }
        return QString();
    }
    long long numberValue(uint udsField, long long defaultValue = -1) const
    {
        auto it = std::find_if(storage.cbegin(), storage.cend(), [udsField](const Field &entry) {
            return entry.m_index == udsField;
        });
        if (it != storage.cend()) {
            return it->m_long;
        }
        return defaultValue;
    }
    QString spaceUsed()
    {
        return QStringLiteral("size:%1 space used:%2")
            .arg(storage.size() * sizeof(Field) + sizeof(std::vector<Field>))
            .arg(storage.capacity() * sizeof(Field) + sizeof(std::vector<Field>));
    }
};
Q_DECLARE_TYPEINFO(AnotherUDSEntry, Q_RELOCATABLE_TYPE);

// Use one vector with binary search
class AnotherV2UDSEntry
{
private:
    struct Field {
        inline Field()
        {
        }
        inline Field(const uint index, const QString &value) noexcept
            : m_str(value)
            , m_index(index)
        {
        }
        inline Field(const uint index, long long value = 0) noexcept
            : m_long(value)
            , m_index(index)
        {
        }
        // This operator helps to gain 1ms just comparing the key
        inline bool operator==(const Field &other) const
        {
            return m_index == other.m_index;
        }

        QString m_str;
        long long m_long = LLONG_MIN;
        uint m_index = 0;
    };
    std::vector<Field> storage;

private:
    static inline bool less(const Field &other, const uint index)
    {
        return other.m_index < index;
    }

public:
    void reserve(int size)
    {
        storage.reserve(size);
    }
    void insert(uint udsField, const QString &value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
        auto it = std::lower_bound(storage.cbegin(), storage.cend(), udsField, less);
        Q_ASSERT(it == storage.cend() || it->m_index != udsField);
        storage.emplace(it, udsField, value);
    }
    void replaceOrInsert(uint udsField, const QString &value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
        auto it = std::lower_bound(storage.begin(), storage.end(), udsField, less);
        if (it != storage.end() && it->m_index == udsField) {
            it->m_str = value;
            return;
        }
        storage.emplace(it, udsField, value);
    }
    void insert(uint udsField, long long value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
        auto it = std::lower_bound(storage.cbegin(), storage.cend(), udsField, less);
        Q_ASSERT(it == storage.end() || it->m_index != udsField);
        storage.emplace(it, udsField, value);
    }
    void replaceOrInsert(uint udsField, long long value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
        auto it = std::lower_bound(storage.begin(), storage.end(), udsField, less);
        if (it != storage.end() && it->m_index == udsField) {
            it->m_long = value;
            return;
        }
        storage.emplace(it, udsField, value);
    }
    int count() const
    {
        return storage.size();
    }
    QString stringValue(uint udsField) const
    {
        auto it = std::lower_bound(storage.cbegin(), storage.cend(), udsField, less);
        if (it != storage.end() && it->m_index == udsField) {
            return it->m_str;
        }
        return QString();
    }
    long long numberValue(uint udsField, long long defaultValue = -1) const
    {
        auto it = std::lower_bound(storage.cbegin(), storage.cend(), udsField, less);
        if (it != storage.end() && it->m_index == udsField) {
            return it->m_long;
        }
        return defaultValue;
    }

    QString spaceUsed()
    {
        return QStringLiteral("size:%1 space used:%2")
            .arg(storage.size() * sizeof(Field) + sizeof(std::vector<Field>))
            .arg(storage.capacity() * sizeof(Field) + sizeof(std::vector<Field>));
    }
};
Q_DECLARE_TYPEINFO(AnotherV2UDSEntry, Q_RELOCATABLE_TYPE);

// Instead of two vectors, use only one sorted by index and accessed using a binary search.
class TwoVectorKindEntry
{
private:
    struct StringField {
        inline StringField()
        {
        }
        inline StringField(const uint index, const QString &value) noexcept
            : m_index(index)
            , m_str(value)
        {
        }
        // This operator helps to gain 1ms just comparing the key
        inline bool operator==(const StringField &other) const noexcept
        {
            return m_index == other.m_index;
        }

        uint m_index = 0;
        QString m_str;
    };
    struct NumberField {
        inline NumberField() noexcept
        {
        }
        inline NumberField(const uint index, long long value = 0) noexcept
            : m_index(index)
            , m_long(value)
        {
        }
        // This operator helps to gain 1ms just comparing the key
        inline bool operator==(const NumberField &other) const noexcept
        {
            return m_index == other.m_index;
        }

        uint m_index = 0;
        long long m_long = LLONG_MIN;
    };
    std::vector<StringField> stringStorage;
    std::vector<NumberField> numberStorage;

public:
    void reserve(int size)
    {
        Q_UNUSED(size)
        // ideal case
        stringStorage.reserve(3);
        numberStorage.reserve(5);
        // stringStorage.reserve(size / 3);
        // numberStorage.reserve(size * 2 / 3);
    }
    void insert(uint udsField, const QString &value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
        stringStorage.emplace_back(udsField, value);
    }
    void replaceOrInsert(uint udsField, const QString &value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
        auto it = std::find_if(stringStorage.begin(), stringStorage.end(), [udsField](const StringField &field) {
            return field.m_index == udsField;
        });
        if (it != stringStorage.end()) {
            it->m_str = value;
            return;
        }
        stringStorage.emplace(it, udsField, value);
    }
    QString stringValue(uint udsField) const
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_STRING);
        auto it = std::find_if(stringStorage.cbegin(), stringStorage.cend(), [udsField](const StringField &field) {
            return field.m_index == udsField;
        });
        if (it != stringStorage.end()) {
            return it->m_str;
        }
        return QString();
    }
    void insert(uint udsField, long long value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
        numberStorage.emplace_back(udsField, value);
    }
    void replaceOrInsert(uint udsField, long long value)
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
        auto it = std::find_if(numberStorage.begin(), numberStorage.end(), [udsField](const NumberField &field) {
            return field.m_index == udsField;
        });
        if (it != numberStorage.end()) {
            it->m_long = value;
            return;
        }
        numberStorage.emplace(it, udsField, value);
    }
    long long numberValue(uint udsField, long long defaultValue = -1) const
    {
        Q_ASSERT(udsField & KIO::UDSEntry::UDS_NUMBER);
        auto it = std::find_if(numberStorage.cbegin(), numberStorage.cend(), [udsField](const NumberField &field) {
            return field.m_index == udsField;
        });
        if (it != numberStorage.end()) {
            return it->m_long;
        }
        return defaultValue;
    }

    int count() const
    {
        return stringStorage.size() + numberStorage.size();
    }

    QString spaceUsed()
    {
        return QStringLiteral("size:%1 space used:%2")
            .arg(stringStorage.size() * sizeof(StringField) + sizeof(std::vector<StringField>) + sizeof(std::vector<NumberField>)
                 + numberStorage.size() * sizeof(NumberField))
            .arg(sizeof(std::vector<StringField>) + sizeof(std::vector<NumberField>) + stringStorage.capacity() * sizeof(StringField)
                 + numberStorage.capacity() * sizeof(NumberField));
    }
};
Q_DECLARE_TYPEINFO(TwoVectorKindEntry, Q_MOVABLE_TYPE);

// Single vector like AnotherUDSEntry, but the value is a union discriminated by the UDS_STRING bit
// already present in the index. This halves the per-field footprint (no unused QString next to every
// number, no unused long long next to every string) and skips constructing a QString for number
// fields entirely.
class UnionUDSEntry
{
private:
    struct Field {
        uint m_index = 0;
        union {
            long long m_long;
            QString m_str;
        };

        inline bool isString() const noexcept
        {
            return m_index & KIO::UDSEntry::UDS_STRING;
        }

        inline Field() noexcept
            : m_long(0)
        {
        }
        inline Field(const uint index, const QString &value)
            : m_index(index)
        {
            new (&m_str) QString(value);
        }
        inline Field(const uint index, long long value) noexcept
            : m_index(index)
            , m_long(value)
        {
        }
        inline Field(const Field &other)
            : m_index(other.m_index)
        {
            if (other.isString()) {
                new (&m_str) QString(other.m_str);
            } else {
                m_long = other.m_long;
            }
        }
        inline Field(Field &&other) noexcept
            : m_index(other.m_index)
        {
            if (other.isString()) {
                new (&m_str) QString(std::move(other.m_str));
            } else {
                m_long = other.m_long;
            }
        }
        inline Field &operator=(const Field &other)
        {
            if (this != &other) {
                if (isString()) {
                    m_str.~QString();
                }
                m_index = other.m_index;
                if (other.isString()) {
                    new (&m_str) QString(other.m_str);
                } else {
                    m_long = other.m_long;
                }
            }
            return *this;
        }
        inline Field &operator=(Field &&other) noexcept
        {
            if (this != &other) {
                if (isString()) {
                    m_str.~QString();
                }
                m_index = other.m_index;
                if (other.isString()) {
                    new (&m_str) QString(std::move(other.m_str));
                } else {
                    m_long = other.m_long;
                }
            }
            return *this;
        }
        inline ~Field()
        {
            if (isString()) {
                m_str.~QString();
            }
        }
    };
    std::vector<Field> storage;

public:
    void reserve(int size)
    {
        storage.reserve(size);
    }
    void insert(uint udsField, const QString &value)
    {
        storage.emplace_back(udsField, value);
    }
    void replaceOrInsert(uint udsField, const QString &value)
    {
        for (Field &f : storage) {
            if (f.m_index == udsField) {
                f.m_str = value;
                return;
            }
        }
        storage.emplace_back(udsField, value);
    }
    void insert(uint udsField, long long value)
    {
        storage.emplace_back(udsField, value);
    }
    void replaceOrInsert(uint udsField, long long value)
    {
        for (Field &f : storage) {
            if (f.m_index == udsField) {
                f.m_long = value;
                return;
            }
        }
        storage.emplace_back(udsField, value);
    }
    int count() const
    {
        return storage.size();
    }
    QString stringValue(uint udsField) const
    {
        for (const Field &f : storage) {
            if (f.m_index == udsField) {
                return f.m_str;
            }
        }
        return QString();
    }
    long long numberValue(uint udsField, long long defaultValue = -1) const
    {
        for (const Field &f : storage) {
            if (f.m_index == udsField) {
                return f.m_long;
            }
        }
        return defaultValue;
    }
    QString spaceUsed()
    {
        return QStringLiteral("size:%1 space used:%2")
            .arg(storage.size() * sizeof(Field) + sizeof(std::vector<Field>))
            .arg(storage.capacity() * sizeof(Field) + sizeof(std::vector<Field>));
    }
};
Q_DECLARE_TYPEINFO(UnionUDSEntry, Q_RELOCATABLE_TYPE);

// The entries of a listing carry the same fields in the same order, so the sequence of their ids is
// interned and shared between them, and an entry keeps its values alone. Inserting a field walks a
// tree of the sequences seen so far, one step per field, which every entry after the first follows
// without making anything.
// The fields an entry carries, in the order they were given, shared by every entry carrying them.
struct FieldSequence {
    std::vector<uint> stringFields;
    std::vector<uint> numberFields;
    // The sequences that carry these fields and one more.
    mutable std::vector<std::pair<uint, const FieldSequence *>> longer;
};

// A sequence is kept for as long as the program runs, there being one for each shape an entry takes,
// so an entry points at it without counting who else does.
static const FieldSequence *emptySequence()
{
    static thread_local const FieldSequence *sequence = new FieldSequence;
    return sequence;
}

static const FieldSequence *sequenceWith(const FieldSequence *from, uint field, bool isString)
{
    for (const auto &[knownField, longer] : from->longer) {
        if (knownField == field) {
            return longer;
        }
    }
    FieldSequence *longer = new FieldSequence{from->stringFields, from->numberFields, {}};
    if (isString) {
        longer->stringFields.push_back(field);
    } else {
        longer->numberFields.push_back(field);
    }
    from->longer.emplace_back(field, longer);
    return longer;
}

class InternedFieldsUDSEntry
{
public:
public:
    InternedFieldsUDSEntry()
        : m_schema(emptySequence())
    {
    }

    void reserve(int size)
    {
        // The caller says how many fields it is about to insert without saying of which kind, and a
        // listing carries about two numbers for every string.
        m_strings.reserve(size / 3);
        m_numbers.reserve(size * 2 / 3);
    }

    void insert(uint field, const QString &value)
    {
        m_schema = sequenceWith(m_schema, field, true);
        m_strings.push_back(value);
    }

    void insert(uint field, long long value)
    {
        m_schema = sequenceWith(m_schema, field, false);
        m_numbers.push_back(value);
    }

    int count() const
    {
        return int(m_strings.size() + m_numbers.size());
    }

    QString stringValue(uint field) const
    {
        const std::vector<uint> &fields = m_schema->stringFields;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i] == field) {
                return m_strings[i];
            }
        }
        return QString();
    }

    long long numberValue(uint field, long long defaultValue = -1) const
    {
        const std::vector<uint> &fields = m_schema->numberFields;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i] == field) {
                return m_numbers[i];
            }
        }
        return defaultValue;
    }

    QString spaceUsed()
    {
        const size_t fixed = sizeof(const FieldSequence *) + sizeof(std::vector<QString>) + sizeof(std::vector<long long>);
        return QStringLiteral("size:%1 space used:%2")
            .arg(m_strings.size() * sizeof(QString) + m_numbers.size() * sizeof(long long) + fixed)
            .arg(m_strings.capacity() * sizeof(QString) + m_numbers.capacity() * sizeof(long long) + fixed);
    }

private:
    const FieldSequence *m_schema;
    std::vector<QString> m_strings;
    std::vector<long long> m_numbers;
};

// The same set of fields shared between entries, with the values of an entry in a single block, the
// strings first and the numbers after them. A listing builds such an entry once from what it
// receives, so growing one field at a time, as this does, is not what the shape is for.
class InternedSingleBlockUDSEntry
{
public:
    InternedSingleBlockUDSEntry() = default;

    InternedSingleBlockUDSEntry(const InternedSingleBlockUDSEntry &other)
    {
        copyFrom(other);
    }

    InternedSingleBlockUDSEntry &operator=(const InternedSingleBlockUDSEntry &other)
    {
        if (this != &other) {
            release();
            copyFrom(other);
        }
        return *this;
    }

    ~InternedSingleBlockUDSEntry()
    {
        release();
    }

    void reserve(int size)
    {
        Q_UNUSED(size)
    }

    void insert(uint field, const QString &value)
    {
        const size_t strings = m_fields->stringFields.size();
        const size_t numbers = m_fields->numberFields.size();
        void *block = ::operator new((strings + 1) * sizeof(QString) + numbers * sizeof(long long));
        QString *newStrings = static_cast<QString *>(block);
        for (size_t i = 0; i < strings; ++i) {
            new (newStrings + i) QString(std::move(stringSlot(i)));
        }
        new (newStrings + strings) QString(value);
        long long *newNumbers = reinterpret_cast<long long *>(newStrings + strings + 1);
        for (size_t i = 0; i < numbers; ++i) {
            newNumbers[i] = numberSlot(i);
        }
        release();
        m_block = block;
        m_fields = sequenceWith(m_fields, field, true);
    }

    void insert(uint field, long long value)
    {
        const size_t strings = m_fields->stringFields.size();
        const size_t numbers = m_fields->numberFields.size();
        void *block = ::operator new(strings * sizeof(QString) + (numbers + 1) * sizeof(long long));
        QString *newStrings = static_cast<QString *>(block);
        for (size_t i = 0; i < strings; ++i) {
            new (newStrings + i) QString(std::move(stringSlot(i)));
        }
        long long *newNumbers = reinterpret_cast<long long *>(newStrings + strings);
        for (size_t i = 0; i < numbers; ++i) {
            newNumbers[i] = numberSlot(i);
        }
        newNumbers[numbers] = value;
        release();
        m_block = block;
        m_fields = sequenceWith(m_fields, field, false);
    }

    int count() const
    {
        return int(m_fields->stringFields.size() + m_fields->numberFields.size());
    }

    QString stringValue(uint field) const
    {
        const std::vector<uint> &fields = m_fields->stringFields;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i] == field) {
                return stringSlot(i);
            }
        }
        return QString();
    }

    long long numberValue(uint field, long long defaultValue = -1) const
    {
        const std::vector<uint> &fields = m_fields->numberFields;
        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i] == field) {
                return numberSlot(i);
            }
        }
        return defaultValue;
    }

    QString spaceUsed()
    {
        const size_t block = m_fields->stringFields.size() * sizeof(QString) + m_fields->numberFields.size() * sizeof(long long);
        const size_t fixed = sizeof(const FieldSequence *) + sizeof(void *);
        return QStringLiteral("size:%1 space used:%2").arg(block + fixed).arg(block + fixed);
    }

private:
    QString &stringSlot(size_t index) const
    {
        return *(static_cast<QString *>(m_block) + index);
    }

    long long &numberSlot(size_t index) const
    {
        return *(reinterpret_cast<long long *>(static_cast<QString *>(m_block) + m_fields->stringFields.size()) + index);
    }

    void release()
    {
        if (!m_block) {
            return;
        }
        for (size_t i = 0; i < m_fields->stringFields.size(); ++i) {
            stringSlot(i).~QString();
        }
        ::operator delete(m_block);
        m_block = nullptr;
    }

    void copyFrom(const InternedSingleBlockUDSEntry &other)
    {
        m_fields = other.m_fields;
        m_block = nullptr;
        if (!other.m_block) {
            return;
        }
        const size_t strings = m_fields->stringFields.size();
        const size_t numbers = m_fields->numberFields.size();
        m_block = ::operator new(strings * sizeof(QString) + numbers * sizeof(long long));
        for (size_t i = 0; i < strings; ++i) {
            new (static_cast<QString *>(m_block) + i) QString(other.stringSlot(i));
        }
        for (size_t i = 0; i < numbers; ++i) {
            numberSlot(i) = other.numberSlot(i);
        }
    }

    const FieldSequence *m_fields = emptySequence();
    void *m_block = nullptr;
};

template<class T>
static void fillUDSEntries(T &entry, time_t now_time_t, const QString &nameStr, const QString &groupStr)
{
    entry.reserve(8);
    // In random order of index
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 123456ULL);
    entry.insert(KIO::UDSEntry::UDS_NAME, nameStr);
    entry.insert(KIO::UDSEntry::UDS_GROUP, groupStr);
    entry.insert(KIO::UDSEntry::UDS_USER, nameStr);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0644);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
}

template<class T>
void testFill(UdsEntryBenchmark *bench)
{
    // test fill, aka append to container efficiency
    QBENCHMARK {
        T entry;
        fillUDSEntries<T>(entry, bench->now_time_t, bench->nameStr, bench->groupStr);
    }
}

template<class T>
void testCompare(UdsEntryBenchmark *bench)
{
    // heavy read test, aka container access efficiency
    T entry;
    T entry2;
    fillUDSEntries<T>(entry, bench->now_time_t, bench->nameStr, bench->groupStr);
    fillUDSEntries<T>(entry2, bench->now_time_t, bench->nameStr, bench->groupStr);
    QCOMPARE(entry.count(), 8);
    QCOMPARE(entry2.count(), 8);
    QBENCHMARK {
        bool equal = entry.stringValue(KIO::UDSEntry::UDS_NAME) == entry2.stringValue(KIO::UDSEntry::UDS_NAME)
            && entry.numberValue(KIO::UDSEntry::UDS_SIZE) == entry2.numberValue(KIO::UDSEntry::UDS_SIZE)
            && entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME) == entry2.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME)
            && entry.numberValue(KIO::UDSEntry::UDS_ACCESS_TIME) == entry2.numberValue(KIO::UDSEntry::UDS_ACCESS_TIME)
            && entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE) == entry2.numberValue(KIO::UDSEntry::UDS_FILE_TYPE)
            && entry.numberValue(KIO::UDSEntry::UDS_ACCESS) == entry2.numberValue(KIO::UDSEntry::UDS_ACCESS)
            && entry.stringValue(KIO::UDSEntry::UDS_USER) == entry2.stringValue(KIO::UDSEntry::UDS_USER)
            && entry.stringValue(KIO::UDSEntry::UDS_GROUP) == entry2.stringValue(KIO::UDSEntry::UDS_GROUP);
        QVERIFY(equal);
    }
}

template<class T>
void testApp(UdsEntryBenchmark *bench)
{
    // test fill + entry read

    QString displayName;
    KIO::filesize_t size;
    int access;
    QString url;

    QBENCHMARK {
        T entry;
        fillUDSEntries<T>(entry, bench->now_time_t, bench->nameStr, bench->groupStr);

        // random field access
        displayName = entry.stringValue(KIO::UDSEntry::UDS_NAME);
        url = entry.stringValue(KIO::UDSEntry::UDS_URL);
        size = entry.numberValue(KIO::UDSEntry::UDS_SIZE);
        access = entry.numberValue(KIO::UDSEntry::UDS_ACCESS);
        QCOMPARE(size, 123456ULL);
        QCOMPARE(access, 0644);
        QCOMPARE(displayName, QStringLiteral("name"));
        QVERIFY(url.isEmpty());
    }
}

template<class T>
void testStruct(UdsEntryBenchmark *bench)
{
    testFill<T>(bench);
    testCompare<T>(bench);
    testApp<T>(bench);
}

void UdsEntryBenchmark::testAnotherFill()
{
    testFill<AnotherUDSEntry>(this);
}
void UdsEntryBenchmark::testTwoVectorKindEntryFill()
{
    testFill<TwoVectorKindEntry>(this);
}
void UdsEntryBenchmark::testAnotherV2Fill()
{
    testFill<AnotherV2UDSEntry>(this);
}
void UdsEntryBenchmark::testTwoVectorsFill()
{
    testFill<FrankUDSEntry>(this);
}
void UdsEntryBenchmark::testUDSEntryHSFill()
{
    testFill<UDSEntryHS>(this);
}

void UdsEntryBenchmark::testAnotherCompare()
{
    testCompare<AnotherUDSEntry>(this);
}
void UdsEntryBenchmark::testAnotherV2Compare()
{
    testCompare<AnotherV2UDSEntry>(this);
}
void UdsEntryBenchmark::testTwoVectorKindEntryCompare()
{
    testCompare<TwoVectorKindEntry>(this);
}
void UdsEntryBenchmark::testTwoVectorsCompare()
{
    testCompare<FrankUDSEntry>(this);
}
void UdsEntryBenchmark::testUDSEntryHSCompare()
{
    testCompare<UDSEntryHS>(this);
}

void UdsEntryBenchmark::testTwoVectorKindEntryApp()
{
    testApp<TwoVectorKindEntry>(this);
}
void UdsEntryBenchmark::testAnotherApp()
{
    testApp<AnotherUDSEntry>(this);
}
void UdsEntryBenchmark::testAnotherV2App()
{
    testApp<AnotherV2UDSEntry>(this);
}
void UdsEntryBenchmark::testTwoVectorsApp()
{
    testApp<FrankUDSEntry>(this);
}
void UdsEntryBenchmark::testUDSEntryHSApp()
{
    testApp<UDSEntryHS>(this);
}

void UdsEntryBenchmark::testUnionFill()
{
    testFill<UnionUDSEntry>(this);
}
void UdsEntryBenchmark::testUnionCompare()
{
    testCompare<UnionUDSEntry>(this);
}
void UdsEntryBenchmark::testUnionApp()
{
    testApp<UnionUDSEntry>(this);
}

template<class T>
void printSpaceUsed(UdsEntryBenchmark *bench)
{
    T entry;
    fillUDSEntries<T>(entry, bench->now_time_t, bench->nameStr, bench->groupStr);
    qDebug() << typeid(T).name() << " memory used" << entry.spaceUsed();
}

void UdsEntryBenchmark::testInternedFieldsFill()
{
    testFill<InternedFieldsUDSEntry>(this);
}

void UdsEntryBenchmark::testInternedFieldsCompare()
{
    testCompare<InternedFieldsUDSEntry>(this);
}

void UdsEntryBenchmark::testInternedFieldsApp()
{
    testApp<InternedFieldsUDSEntry>(this);
}

// The resident memory of this process, read back after asking the allocator to return what it can.
static long processResidentBytes()
{
    QFile status(QStringLiteral("/proc/self/status"));
    if (!status.open(QIODevice::ReadOnly)) {
        return -1;
    }
    for (QByteArray line = status.readLine(); !line.isEmpty(); line = status.readLine()) {
        if (line.startsWith("VmRSS:")) {
            return line.mid(6).simplified().split(' ').first().toLong() * 1024;
        }
    }
    return -1;
}

// The fields a listing carries for a file, as kio_file sends them: two strings, the name and where a
// link points, and eight numbers, the type, the permissions, the size, the three times and the two
// ids of who owns it.
template<class T>
static void fillDolphinLikeEntry(T &entry, time_t now_time_t, const QString &nameStr, const QString &groupStr)
{
    entry.reserve(10);
    entry.insert(KIO::UDSEntry::UDS_NAME, nameStr);
    entry.insert(KIO::UDSEntry::UDS_LINK_DEST, groupStr);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0644);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 123456ULL);
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_CREATION_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_LOCAL_USER_ID, 1000);
    entry.insert(KIO::UDSEntry::UDS_LOCAL_GROUP_ID, 1000);
}

// What a window listing a folder does: it keeps an entry per file for as long as the folder is
// shown, and reads a few of their fields again on every sort, filter and repaint.
template<class T>
static void dolphinLikeWorkload(UdsEntryBenchmark *bench, const char *name)
{
    const int fileCount = 100000;
    const int viewPasses = 20;

    QList<T> entries;
    entries.reserve(fileCount);

#if defined(__GLIBC__)
    malloc_trim(0);
#endif
    const long before = processResidentBytes();

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < fileCount; ++i) {
        entries.append(T());
        fillDolphinLikeEntry<T>(entries.last(), bench->now_time_t, bench->nameStr, bench->groupStr);
    }
    const qint64 fillMs = timer.elapsed();

#if defined(__GLIBC__)
    malloc_trim(0);
#endif
    const long after = processResidentBytes();

    timer.restart();
    long long sink = 0;
    for (int pass = 0; pass < viewPasses; ++pass) {
        for (const T &entry : std::as_const(entries)) {
            sink += entry.stringValue(KIO::UDSEntry::UDS_NAME).size();
            sink += entry.numberValue(KIO::UDSEntry::UDS_SIZE);
            sink += entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME);
            sink += entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE);
        }
    }
    const qint64 readMs = timer.elapsed();

    QVERIFY(sink > 0);
    qInfo().nospace() << name << ": keeping " << fileCount << " entries took " << fillMs << " ms and " << (after - before) << " bytes, "
                      << double(after - before) / fileCount << " bytes per entry. Reading four fields of each, " << viewPasses << " times over, took " << readMs
                      << " ms.";
}

void UdsEntryBenchmark::testInternedSingleBlockFill()
{
    testFill<InternedSingleBlockUDSEntry>(this);
}

void UdsEntryBenchmark::testInternedSingleBlockCompare()
{
    testCompare<InternedSingleBlockUDSEntry>(this);
}

void UdsEntryBenchmark::testInternedSingleBlockApp()
{
    testApp<InternedSingleBlockUDSEntry>(this);
}

void UdsEntryBenchmark::testspaceUsed()
{
    printSpaceUsed<FrankUDSEntry>(this);
    printSpaceUsed<AnotherUDSEntry>(this);
    printSpaceUsed<AnotherV2UDSEntry>(this);
    printSpaceUsed<TwoVectorKindEntry>(this);
    printSpaceUsed<UDSEntryHS>(this);
    printSpaceUsed<UnionUDSEntry>(this);
    printSpaceUsed<InternedFieldsUDSEntry>(this);
    printSpaceUsed<InternedSingleBlockUDSEntry>(this);
}

void UdsEntryBenchmark::testDolphinLikeWorkload()
{
    dolphinLikeWorkload<FrankUDSEntry>(this, "two vectors");
    dolphinLikeWorkload<AnotherUDSEntry>(this, "one vector");
    dolphinLikeWorkload<AnotherV2UDSEntry>(this, "one vector, v2");
    dolphinLikeWorkload<TwoVectorKindEntry>(this, "two vectors by kind, as it is now");
    dolphinLikeWorkload<UDSEntryHS>(this, "hash");
    dolphinLikeWorkload<UnionUDSEntry>(this, "union");
    dolphinLikeWorkload<InternedFieldsUDSEntry>(this, "interned set of fields");
    dolphinLikeWorkload<InternedSingleBlockUDSEntry>(this, "interned, values in one block");
}

QTEST_MAIN(UdsEntryBenchmark)

#include "udsentry_api_comparison_benchmark.moc"
