/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2014 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2024 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <QHash>
#include <QList>

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

    void testAnotherCompare();
    void testTwoVectorKindEntryCompare();
    void testAnotherV2Compare();
    void testTwoVectorsCompare();
    void testUDSEntryHSCompare();

    void testAnotherApp();
    void testTwoVectorKindEntryApp();
    void testAnotherV2App();
    void testTwoVectorsApp();
    void testUDSEntryHSApp();

    void testspaceUsed();

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
        inline Field()
        {
        }
        inline Field(const uint index, const QString &value)
            : m_str(value)
            , m_index(index)
        {
        }
        inline Field(const uint index, long long value = 0)
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
        inline Field(const uint index, const QString &value)
            : m_str(value)
            , m_index(index)
        {
        }
        inline Field(const uint index, long long value = 0)
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
        inline StringField(const uint index, const QString &value)
            : m_index(index)
            , m_str(value)
        {
        }
        // This operator helps to gain 1ms just comparing the key
        inline bool operator==(const StringField &other) const
        {
            return m_index == other.m_index;
        }

        uint m_index = 0;
        QString m_str;
    };
    struct NumberField {
        inline NumberField()
        {
        }
        inline NumberField(const uint index, long long value = 0)
            : m_index(index)
            , m_long(value)
        {
        }
        // This operator helps to gain 1ms just comparing the key
        inline bool operator==(const NumberField &other) const
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

template<class T>
void printSpaceUsed(UdsEntryBenchmark *bench)
{
    T entry;
    fillUDSEntries<T>(entry, bench->now_time_t, bench->nameStr, bench->groupStr);
    qDebug() << typeid(T).name() << " memory used" << entry.spaceUsed();
}

void UdsEntryBenchmark::testspaceUsed()
{
    printSpaceUsed<FrankUDSEntry>(this);
    printSpaceUsed<AnotherUDSEntry>(this);
    printSpaceUsed<AnotherV2UDSEntry>(this);
    printSpaceUsed<TwoVectorKindEntry>(this);
    printSpaceUsed<UDSEntryHS>(this);
}

QTEST_MAIN(UdsEntryBenchmark)

#include "udsentry_api_comparison_benchmark.moc"
