/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "udsentrytest.h"

#include <QTest>
#include <QVector>
#include <QDataStream>
#include <QTemporaryFile>
#include <qplatformdefs.h>

#include <kfileitem.h>
#include <udsentry.h>

#include "kiotesthelper.h"

struct UDSTestField {
    UDSTestField() {}

    UDSTestField(uint uds, const QString &value) :
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
    const QVector<QVector<UDSTestField> > testCases {
        // Data for 1st UDSEntry.
        {
        UDSTestField(KIO::UDSEntry::UDS_SIZE, 1),
        UDSTestField(KIO::UDSEntry::UDS_USER, QStringLiteral("user1")),
        UDSTestField(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1")),
        UDSTestField(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename1")),
        UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 123456),
        UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 12345),
        UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 2),
        UDSTestField(KIO::UDSEntry::UDS_INODE, 56)
        },
        // 2nd entry: change some of the data.
        {
        UDSTestField(KIO::UDSEntry::UDS_SIZE, 2),
        UDSTestField(KIO::UDSEntry::UDS_USER, QStringLiteral("user2")),
        UDSTestField(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1")),
        UDSTestField(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename2")),
        UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12345),
        UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1234),
        UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 87),
        UDSTestField(KIO::UDSEntry::UDS_INODE, 42)
        },
        // 3rd entry: keep the data, but change the order of the entries.
        {
        UDSTestField(KIO::UDSEntry::UDS_SIZE, 2),
        UDSTestField(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1")),
        UDSTestField(KIO::UDSEntry::UDS_USER, QStringLiteral("user2")),
        UDSTestField(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename2")),
        UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12345),
        UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 87),
        UDSTestField(KIO::UDSEntry::UDS_INODE, 42),
        UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1234),
        },
        // 4th entry: change some of the data and the order of the entries.
        {
        UDSTestField(KIO::UDSEntry::UDS_SIZE, 2),
        UDSTestField(KIO::UDSEntry::UDS_USER, QStringLiteral("user4")),
        UDSTestField(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group4")),
        UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12346),
        UDSTestField(KIO::UDSEntry::UDS_DEVICE_ID, 87),
        UDSTestField(KIO::UDSEntry::UDS_INODE, 42),
        UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1235),
        UDSTestField(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename4"))
        },
        // 5th entry: remove one field.
        {
        UDSTestField(KIO::UDSEntry::UDS_SIZE, 2),
        UDSTestField(KIO::UDSEntry::UDS_USER, QStringLiteral("user4")),
        UDSTestField(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group4")),
        UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12346),
        UDSTestField(KIO::UDSEntry::UDS_INODE, 42),
        UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1235),
        UDSTestField(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename4"))
        },
        // 6th entry: add a new field, and change some others.
        {
        UDSTestField(KIO::UDSEntry::UDS_SIZE, 89),
        UDSTestField(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("icon6")),
        UDSTestField(KIO::UDSEntry::UDS_USER, QStringLiteral("user6")),
        UDSTestField(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group4")),
        UDSTestField(KIO::UDSEntry::UDS_MODIFICATION_TIME, 12346),
        UDSTestField(KIO::UDSEntry::UDS_INODE, 32),
        UDSTestField(KIO::UDSEntry::UDS_CREATION_TIME, 1235),
        UDSTestField(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename6"))
        }
    };
    // Store the entries in a QByteArray.
    QByteArray data;
    {
        QDataStream stream(&data, QIODevice::WriteOnly);
        for (const QVector<UDSTestField> &testCase : testCases) {
            KIO::UDSEntry entry;

            for (const UDSTestField &field : testCase) {
                uint uds = field.m_uds;
                if (uds & KIO::UDSEntry::UDS_STRING) {
                    entry.fastInsert(uds, field.m_string);
                } else {
                    Q_ASSERT(uds & KIO::UDSEntry::UDS_NUMBER);
                    entry.fastInsert(uds, field.m_long);
                }
            }

            QCOMPARE(entry.count(), testCase.count());
            stream << entry;
        }
    }

    // Re-load the entries and compare with the data in testCases.
    {
        QDataStream stream(data);
        for (const QVector<UDSTestField> &testCase : testCases) {
            KIO::UDSEntry entry;
            stream >> entry;
            QCOMPARE(entry.count(), testCase.count());

            for (const UDSTestField &field : testCase) {
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
        for (const QVector<UDSTestField> &testCase : testCases) {
            stream << testCase.count();

            for (const UDSTestField &field : testCase) {
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
        for (const QVector<UDSTestField> &testCase : testCases) {
            KIO::UDSEntry entry;
            stream >> entry;
            QCOMPARE(entry.count(), testCase.count());

            for (const UDSTestField &field : testCase) {
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

/**
 * Test to verify that move semantics work. This is only useful when ran through callgrind.
 */
void UDSEntryTest::testMove()
{
    // Create a temporary file. Just to make a UDSEntry further down.
    QTemporaryFile file;
    QVERIFY(file.open());
    const QByteArray filePath = file.fileName().toLocal8Bit();
    const QString fileName = QUrl(file.fileName()).fileName(); // QTemporaryFile::fileName returns the full path.
    QVERIFY(!fileName.isEmpty());

    // We have a file now. Get the stat data from it to make the UDSEntry.
    QT_STATBUF statBuf;
    QVERIFY(QT_LSTAT(filePath.constData(), &statBuf) == 0);
    KIO::UDSEntry entry(statBuf, fileName);

    // Verify that the name in the UDSEntry is the same as we've got from the fileName var.
    QCOMPARE(fileName, entry.stringValue(KIO::UDSEntry::UDS_NAME));

    // That was the boilerplate code. Now for move semantics.
    // First: move assignment.
    {
        // First a copy as we need to keep the entry for the next test.
        KIO::UDSEntry entryCopy = entry;

        // Now move-assignment (two lines to prevent compiler optimization)
        KIO::UDSEntry movedEntry;
        movedEntry = std::move(entryCopy);

        // And verify that this works.
        QCOMPARE(fileName, movedEntry.stringValue(KIO::UDSEntry::UDS_NAME));
    }

    // Move constructor
    {
        // First a copy again
        KIO::UDSEntry entryCopy = entry;

        // Now move-assignment
        KIO::UDSEntry movedEntry(std::move(entryCopy));

        // And verify that this works.
        QCOMPARE(fileName, movedEntry.stringValue(KIO::UDSEntry::UDS_NAME));
    }
}

/**
 * Test to verify that equal semantics work.
 */
void UDSEntryTest::testEquality()
{
    KIO::UDSEntry entry;
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 1);
    entry.fastInsert(KIO::UDSEntry::UDS_USER, QStringLiteral("user1"));
    entry.fastInsert(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1"));
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename1"));
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, 123456);
    entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, 12345);
    entry.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, 2);
    entry.fastInsert(KIO::UDSEntry::UDS_INODE, 56);

    // Same as entry
    KIO::UDSEntry entry2;
    entry2.fastInsert(KIO::UDSEntry::UDS_SIZE, 1);
    entry2.fastInsert(KIO::UDSEntry::UDS_USER, QStringLiteral("user1"));
    entry2.fastInsert(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1"));
    entry2.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename1"));
    entry2.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, 123456);
    entry2.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, 12345);
    entry2.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, 2);
    entry2.fastInsert(KIO::UDSEntry::UDS_INODE, 56);

    // 3nd entry: different user.
    KIO::UDSEntry entry3;
    entry3.fastInsert(KIO::UDSEntry::UDS_SIZE, 1);
    entry3.fastInsert(KIO::UDSEntry::UDS_USER, QStringLiteral("other user"));
    entry3.fastInsert(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1"));
    entry3.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename1"));
    entry3.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, 123456);
    entry3.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, 12345);
    entry3.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, 2);
    entry3.fastInsert(KIO::UDSEntry::UDS_INODE, 56);

    // 4th entry : an additional field
    KIO::UDSEntry entry4;
    entry4.fastInsert(KIO::UDSEntry::UDS_SIZE, 1);
    entry4.fastInsert(KIO::UDSEntry::UDS_USER, QStringLiteral("user1"));
    entry4.fastInsert(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1"));
    entry4.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("filename1"));
    entry4.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("home"));
    entry4.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, 123456);
    entry4.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, 12345);
    entry4.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, 2);
    entry4.fastInsert(KIO::UDSEntry::UDS_INODE, 56);

    // ==
    QVERIFY(entry == entry2);
    QVERIFY(!(entry == entry3));
    QVERIFY(!(entry == entry4));
    QVERIFY(!(entry2 == entry3));

    // !=
    QVERIFY(!(entry != entry2));
    QVERIFY(entry != entry3);
    QVERIFY(entry != entry4);
    QVERIFY(entry2 != entry3);

    // make entry3 == entry
    entry3.replace(KIO::UDSEntry::UDS_USER, QStringLiteral("user1"));

    QVERIFY(entry == entry3);
    QVERIFY(entry2 == entry3);
    QVERIFY(!(entry != entry3));
    QVERIFY(!(entry2 != entry3));
}

QTEST_MAIN(UDSEntryTest)
