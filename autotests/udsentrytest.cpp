/* This file is part of the KDE project
   Copyright (C) 2013 Frank Reininghaus <frank78ac@googlemail.com>

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

#include "udsentrytest.h"

#include <QTest>
#include <QVector>

#include <udsentry.h>

struct UDSTestField
{
    UDSTestField() {}

    UDSTestField(uint uds, const QString& value) :
        m_uds(uds),
        m_string(value)
    {
        Q_ASSERT(uds & KIO::UDSEntry::UDS_STRING);
    }

    UDSTestField(uint uds, long long value) :
        m_uds(uds),
        m_long(value)
    {
        Q_ASSERT(uds & KIO::UDSEntry::UDS_NUMBER);
    }

    uint m_uds;
    QString m_string;
    long long m_long;
};

/**
 * Test that storing UDSEntries to a stream and then re-loading them works.
 */
void UDSEntryTest::testSaveLoad()
{
    QVector<QVector<UDSTestField> > testCases;
    QVector<UDSTestField> testCase;

    // Data for 1st UDSEntry.
    testCase
        << UDSTestField(KIO::UDSEntry::UDS_SIZE, 1)
        << UDSTestField(KIO::UDSEntry::UDS_USER, "user1")
        << UDSTestField(KIO::UDSEntry::UDS_GROUP, "group1")
        << UDSTestField(KIO::UDSEntry::UDS_NAME, QLatin1String("filename1"))
        << UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 123456)
        << UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 12345)
        << UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 2)
        << UDSTestField(KIO::UDSEntry::UDS_INODE, 56);
    testCases << testCase;

    // 2nd entry: change some of the data.
    testCase.clear();
    testCase
        << UDSTestField(KIO::UDSEntry::UDS_SIZE, 2)
        << UDSTestField(KIO::UDSEntry::UDS_USER, "user2")
        << UDSTestField(KIO::UDSEntry::UDS_GROUP, "group1")
        << UDSTestField(KIO::UDSEntry::UDS_NAME, QLatin1String("filename2"))
        << UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12345)
        << UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1234)
        << UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 87)
        << UDSTestField(KIO::UDSEntry::UDS_INODE, 42);
    testCases << testCase;

    // 3rd entry: keep the data, but change the order of the entries.
    testCase.clear();
    testCase
        << UDSTestField(KIO::UDSEntry::UDS_SIZE, 2)
        << UDSTestField(KIO::UDSEntry::UDS_GROUP, "group1")
        << UDSTestField(KIO::UDSEntry::UDS_USER, "user2")
        << UDSTestField(KIO::UDSEntry::UDS_NAME, QLatin1String("filename2"))
        << UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12345)
        << UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 87)
        << UDSTestField(KIO::UDSEntry::UDS_INODE, 42)
        << UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1234);
    testCases << testCase;

    // 4th entry: change some of the data and the order of the entries.
    testCase.clear();
    testCase
        << UDSTestField(KIO::UDSEntry::UDS_SIZE, 2)
        << UDSTestField(KIO::UDSEntry::UDS_USER, "user4")
        << UDSTestField(KIO::UDSEntry::UDS_GROUP, "group4")
        << UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12346)
        << UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 87)
        << UDSTestField(KIO::UDSEntry::UDS_INODE, 42)
        << UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1235)
        << UDSTestField(KIO::UDSEntry::UDS_NAME, QLatin1String("filename4"));
    testCases << testCase;

    // 5th entry: remove one field.
    testCase.clear();
    testCase
        << UDSTestField(KIO::UDSEntry::UDS_SIZE, 2)
        << UDSTestField(KIO::UDSEntry::UDS_USER, "user4")
        << UDSTestField(KIO::UDSEntry::UDS_GROUP, "group4")
        << UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12346)
        << UDSTestField(KIO::UDSEntry::UDS_INODE, 42)
        << UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1235)
        << UDSTestField(KIO::UDSEntry::UDS_NAME, QLatin1String("filename4"));
    testCases << testCase;

    // 6th entry: add a new field, and change some others.
    testCase.clear();
    testCase
        << UDSTestField(KIO::UDSEntry::UDS_SIZE, 89)
        << UDSTestField(KIO::UDSEntry::UDS_ICON_NAME, "icon6")
        << UDSTestField(KIO::UDSEntry::UDS_USER, "user6")
        << UDSTestField(KIO::UDSEntry::UDS_GROUP, "group4")
        << UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12346)
        << UDSTestField(KIO::UDSEntry::UDS_INODE, 32)
        << UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1235)
        << UDSTestField(KIO::UDSEntry::UDS_NAME, QLatin1String("filename6"));
    testCases << testCase;

    // Store the entries in a QByteArray.
    QByteArray data;
    {
        QDataStream stream(&data, QIODevice::WriteOnly);
        foreach (const QVector<UDSTestField>& testCase, testCases) {
            KIO::UDSEntry entry;

            foreach (const UDSTestField& field, testCase) {
                uint uds = field.m_uds;
                if (uds & KIO::UDSEntry::UDS_STRING) {
                    entry.insert(uds, field.m_string);
                } else {
                    Q_ASSERT(uds & KIO::UDSEntry::UDS_NUMBER);
                    entry.insert(uds, field.m_long);
                }
            }

            QCOMPARE(entry.count(), testCase.count());
            stream << entry;
        }
    }

    // Re-load the entries and compare with the data in testCases.
    {
        QDataStream stream(data);
        foreach (const QVector<UDSTestField>& testCase, testCases) {
            KIO::UDSEntry entry;
            stream >> entry;
            QCOMPARE(entry.count(), testCase.count());

            foreach (const UDSTestField& field, testCase) {
                uint uds = field.m_uds;
                QVERIFY(entry.contains(uds));

                if (uds & KIO::UDSEntry::UDS_STRING) {
                    QCOMPARE(entry.stringValue(uds), field.m_string);
                } else {
                    Q_ASSERT(uds & KIO::UDSEntry::UDS_NUMBER);
                    QCOMPARE(entry.numberValue(uds), field.m_long);
                }
            }
        }
    }

    // Now: Store the fields manually in the order in which they appear in
    // testCases, and re-load them. This ensures that loading the entries
    // works no matter in which order the fields appear in the QByteArray.
    data.clear();

    {
        QDataStream stream(&data, QIODevice::WriteOnly);
        foreach (const QVector<UDSTestField>& testCase, testCases) {
            stream << testCase.count();

            foreach (const UDSTestField& field, testCase) {
                uint uds = field.m_uds;
                stream << uds;

                if (uds & KIO::UDSEntry::UDS_STRING) {
                    stream << field.m_string;
                } else {
                    Q_ASSERT(uds & KIO::UDSEntry::UDS_NUMBER);
                    stream << field.m_long;
                }
            }
        }
    }

    {
        QDataStream stream(data);
        foreach (const QVector<UDSTestField>& testCase, testCases) {
            KIO::UDSEntry entry;
            stream >> entry;
            QCOMPARE(entry.count(), testCase.count());

            foreach (const UDSTestField& field, testCase) {
                uint uds = field.m_uds;
                QVERIFY(entry.contains(uds));

                if (uds & KIO::UDSEntry::UDS_STRING) {
                    QCOMPARE(entry.stringValue(uds), field.m_string);
                } else {
                    Q_ASSERT(uds & KIO::UDSEntry::UDS_NUMBER);
                    QCOMPARE(entry.numberValue(uds), field.m_long);
                }
            }
        }
    }
}

QTEST_MAIN(UDSEntryTest)
