/* This file is part of the KDE project
   Copyright (C) 2004-2014 David Faure <faure@kde.org>

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

#include <qtest.h>

#include <QList>
#include <QVector>
#include <QHash>
#include <QMap>

#include <kio/udsentry.h>
#include <kio/global.h> // filesize_t

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
        : nameStr(QStringLiteral("name")),
          now(QDateTime::currentDateTime()),
          now_time_t(now.toTime_t())
    {}
private Q_SLOTS:
    void testKDE3Slave();
    void testKDE3App();
    void testHashVariantSlave();
    void testHashVariantApp();
    void testHashStructSlave();
    void testHashStructApp();
    void testMapStructSlave();
    void testMapStructApp();
    void testTwoVectorsSlave();
    void testTwoVectorsApp();
private:
    const QString nameStr;
    const QDateTime now;
    const time_t now_time_t;
};

class OldUDSAtom
{
public:
    QString m_str;
    long long m_long;
    unsigned int m_uds;
};
typedef QList<OldUDSAtom> OldUDSEntry; // well it was a QValueList :)

static void fillOldUDSEntry(OldUDSEntry &entry, time_t now_time_t, const QString &nameStr)
{
    OldUDSAtom atom;
    atom.m_uds = KIO::UDSEntry::UDS_NAME;
    atom.m_str = nameStr;
    entry.append(atom);
    atom.m_uds = KIO::UDSEntry::UDS_SIZE;
    atom.m_long = 123456ULL;
    entry.append(atom);
    atom.m_uds = KIO::UDSEntry::UDS_MODIFICATION_TIME;
    atom.m_long = now_time_t;
    entry.append(atom);
    atom.m_uds = KIO::UDSEntry::UDS_ACCESS_TIME;
    atom.m_long = now_time_t;
    entry.append(atom);
    atom.m_uds = KIO::UDSEntry::UDS_FILE_TYPE;
    atom.m_long = S_IFREG;
    entry.append(atom);
    atom.m_uds = KIO::UDSEntry::UDS_ACCESS;
    atom.m_long = 0644;
    entry.append(atom);
    atom.m_uds = KIO::UDSEntry::UDS_USER;
    atom.m_str = nameStr;
    entry.append(atom);
    atom.m_uds = KIO::UDSEntry::UDS_GROUP;
    atom.m_str = nameStr;
    entry.append(atom);
}

void UdsEntryBenchmark::testKDE3Slave()
{
    QBENCHMARK {
        OldUDSEntry entry;
        fillOldUDSEntry(entry, now_time_t, nameStr);
        QCOMPARE(entry.count(), 8);
    }
}

void UdsEntryBenchmark::testKDE3App()
{
    OldUDSEntry entry;
    fillOldUDSEntry(entry, now_time_t, nameStr);

    QString displayName;
    KIO::filesize_t size;
    QString url;

    QBENCHMARK {
        OldUDSEntry::ConstIterator it2 = entry.constBegin();
        for (; it2 != entry.constEnd(); it2++)
        {
            switch ((*it2).m_uds) {
            case KIO::UDSEntry::UDS_NAME:
                displayName = (*it2).m_str;
                break;
            case KIO::UDSEntry::UDS_URL:
                url = (*it2).m_str;
                break;
            case KIO::UDSEntry::UDS_SIZE:
                size = (*it2).m_long;
                break;
            }
        }
        QCOMPARE(size, 123456ULL);
        QCOMPARE(displayName, QStringLiteral("name"));
        QVERIFY(url.isEmpty());
    }
}

// QHash or QMap? doesn't seem to make much difference.
typedef QHash<uint, QVariant> UDSEntryHV;

static void fillUDSEntryHV(UDSEntryHV &entry, const QDateTime &now, const QString &nameStr)
{
    entry.reserve(8);
    entry.insert(KIO::UDSEntry::UDS_NAME, nameStr);
    // we might need a method to make sure people use unsigned long long
    entry.insert(KIO::UDSEntry::UDS_SIZE, 123456ULL);
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, now);
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, now);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0644);
    entry.insert(KIO::UDSEntry::UDS_USER, nameStr);
    entry.insert(KIO::UDSEntry::UDS_GROUP, nameStr);
}

void UdsEntryBenchmark::testHashVariantSlave()
{
    const QDateTime now = QDateTime::currentDateTime();
    QBENCHMARK {
        UDSEntryHV entry;
        fillUDSEntryHV(entry, now, nameStr);
        QCOMPARE(entry.count(), 8);
    }
}

void UdsEntryBenchmark::testHashVariantApp()
{
    // Normally the code would look like this, but let's change it to time it like the old api
    /*
       QString displayName = entry.value( KIO::UDSEntry::UDS_NAME ).toString();
       QUrl url = entry.value( KIO::UDSEntry::UDS_URL ).toString();
       KIO::filesize_t size = entry.value( KIO::UDSEntry::UDS_SIZE ).toULongLong();
     */
    UDSEntryHV entry;
    fillUDSEntryHV(entry, now, nameStr);

    QString displayName;
    KIO::filesize_t size;
    QString url;

    QBENCHMARK {
        // For a field that we assume to always be there
        displayName = entry.value(KIO::UDSEntry::UDS_NAME).toString();

        // For a field that might not be there
        UDSEntryHV::const_iterator it = entry.constFind(KIO::UDSEntry::UDS_URL);
        const UDSEntryHV::const_iterator end = entry.constEnd();
        if (it != end)
        {
            url = it.value().toString();
        }

        it = entry.constFind(KIO::UDSEntry::UDS_SIZE);
        if (it != end)
        {
            size = it.value().toULongLong();
        }

        QCOMPARE(size, 123456ULL);
        QCOMPARE(displayName, QStringLiteral("name"));
        QVERIFY(url.isEmpty());
    }
}

// The KDE4 solution: QHash+struct

// Which one is used depends on UDS_STRING vs UDS_LONG
struct UDSAtom4 { // can't be a union due to qstring...
    UDSAtom4() {} // for QHash or QMap
    UDSAtom4(const QString &s) : m_str(s) {}
    UDSAtom4(long long l) : m_long(l) {}

    QString m_str;
    long long m_long;
};

// Another possibility, to save on QVariant costs
typedef QHash<uint, UDSAtom4> UDSEntryHS; // hash+struct

static void fillQHashStructEntry(UDSEntryHS &entry, time_t now_time_t, const QString &nameStr)
{
    entry.reserve(8);
    entry.insert(KIO::UDSEntry::UDS_NAME, nameStr);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 123456ULL);
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0644);
    entry.insert(KIO::UDSEntry::UDS_USER, nameStr);
    entry.insert(KIO::UDSEntry::UDS_GROUP, nameStr);
}
void UdsEntryBenchmark::testHashStructSlave()
{
    QBENCHMARK {
        UDSEntryHS entry;
        fillQHashStructEntry(entry, now_time_t, nameStr);
        QCOMPARE(entry.count(), 8);
    }
}

void UdsEntryBenchmark::testHashStructApp()
{
    UDSEntryHS entry;
    fillQHashStructEntry(entry, now_time_t, nameStr);

    QString displayName;
    KIO::filesize_t size;
    QString url;

    QBENCHMARK {
        // For a field that we assume to always be there
        displayName = entry.value(KIO::UDSEntry::UDS_NAME).m_str;

        // For a field that might not be there
        UDSEntryHS::const_iterator it = entry.constFind(KIO::UDSEntry::UDS_URL);
        const UDSEntryHS::const_iterator end = entry.constEnd();
        if (it != end)
        {
            url = it.value().m_str;
        }

        it = entry.constFind(KIO::UDSEntry::UDS_SIZE);
        if (it != end)
        {
            size = it.value().m_long;
        }
        QCOMPARE(size, 123456ULL);
        QCOMPARE(displayName, QStringLiteral("name"));
        QVERIFY(url.isEmpty());
    }
}

// Let's see if QMap makes any difference
typedef QMap<uint, UDSAtom4> UDSEntryMS; // map+struct

static void fillQMapStructEntry(UDSEntryMS &entry, time_t now_time_t, const QString &nameStr)
{
    entry.insert(KIO::UDSEntry::UDS_NAME, nameStr);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 123456ULL);
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0644);
    entry.insert(KIO::UDSEntry::UDS_USER, nameStr);
    entry.insert(KIO::UDSEntry::UDS_GROUP, nameStr);
}

void UdsEntryBenchmark::testMapStructSlave()
{
    QBENCHMARK {
        UDSEntryMS entry;
        fillQMapStructEntry(entry, now_time_t, nameStr);
        QCOMPARE(entry.count(), 8);
    }
}

void UdsEntryBenchmark::testMapStructApp()
{
    UDSEntryMS entry;
    fillQMapStructEntry(entry, now_time_t, nameStr);

    QString displayName;
    KIO::filesize_t size;
    QString url;

    QBENCHMARK {

        // For a field that we assume to always be there
        displayName = entry.value(KIO::UDSEntry::UDS_NAME).m_str;

        // For a field that might not be there
        UDSEntryMS::const_iterator it = entry.constFind(KIO::UDSEntry::UDS_URL);
        const UDSEntryMS::const_iterator end = entry.constEnd();
        if (it != end)
        {
            url = it.value().m_str;
        }

        it = entry.constFind(KIO::UDSEntry::UDS_SIZE);
        if (it != end)
        {
            size = it.value().m_long;
        }

        QCOMPARE(size, 123456ULL);
        QCOMPARE(displayName, QStringLiteral("name"));
        QVERIFY(url.isEmpty());
    }
}

// Frank's suggestion in https://git.reviewboard.kde.org/r/118452/
class FrankUDSEntry
{
public:
    class Field
    {
    public:
        inline Field(const QString &value) : m_str(value), m_long(0) {}
        inline Field(long long value = 0) : m_long(value) { }
        QString m_str;
        long long m_long;
    };
    QVector<Field> fields;
    // If udsIndexes[i] == uds, then fields[i] contains the value for 'uds'.
    QVector<uint> udsIndexes;

    void reserve(int size)
    {
        fields.reserve(size);
        udsIndexes.reserve(size);
    }
    void insert(uint field, const QString &value)
    {
        const int index = udsIndexes.indexOf(field);
        if (index >= 0) {
            fields[index] = Field(value);
        } else {
            udsIndexes.append(field);
            fields.append(Field(value));
        }
    }
    void insert(uint field, long long value)
    {
        const int index = udsIndexes.indexOf(field);
        if (index >= 0) {
            fields[index] = Field(value);
        } else {
            udsIndexes.append(field);
            fields.append(Field(value));
        }
    }
    int count() const
    {
        return udsIndexes.count();
    }
    QString stringValue(uint field) const
    {
        const int index = udsIndexes.indexOf(field);
        if (index >= 0) {
            return fields.at(index).m_str;
        } else {
            return QString();
        }
    }
    long long numberValue(uint field, long long defaultValue = -1) const
    {
        const int index = udsIndexes.indexOf(field);
        if (index >= 0) {
            return fields.at(index).m_long;
        } else {
            return defaultValue;
        }
    }
};

static void fillFrankUDSEntry(FrankUDSEntry &entry, time_t now_time_t, const QString &nameStr)
{
    entry.reserve(8);
    entry.insert(KIO::UDSEntry::UDS_NAME, nameStr);
    entry.insert(KIO::UDSEntry::UDS_SIZE, 123456ULL);
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_ACCESS_TIME, now_time_t);
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, 0644);
    entry.insert(KIO::UDSEntry::UDS_USER, nameStr);
    entry.insert(KIO::UDSEntry::UDS_GROUP, nameStr);
}

void UdsEntryBenchmark::testTwoVectorsSlave()
{
    QBENCHMARK {
        FrankUDSEntry entry;
        fillFrankUDSEntry(entry, now_time_t, nameStr);
        QCOMPARE(entry.count(), 8);
    }
}

void UdsEntryBenchmark::testTwoVectorsApp()
{
    FrankUDSEntry entry;
    fillFrankUDSEntry(entry, now_time_t, nameStr);

    QString displayName;
    KIO::filesize_t size;
    QString url;

    QBENCHMARK {
        displayName = entry.stringValue(KIO::UDSEntry::UDS_NAME);
        url = entry.stringValue(KIO::UDSEntry::UDS_URL);
        size = entry.numberValue(KIO::UDSEntry::UDS_SIZE);
        QCOMPARE(size, 123456ULL);
        QCOMPARE(displayName, QStringLiteral("name"));
        QVERIFY(url.isEmpty());
    }
}

QTEST_MAIN(UdsEntryBenchmark)

#include "udsentry_benchmark.moc"
