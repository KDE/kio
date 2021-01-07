/*
    SPDX-FileCopyrightText: 2014 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <kio/udsentry.h>

#include <QTest>

/**
 * This benchmarks tests four typical uses of UDSEntry:
 *
 * (a)  Store data in UDSEntries using
 *      UDSEntry::insert(uint, const QString&) and
 *      UDSEntry::insert(uint, long long),
 *      and append the entries to a UDSEntryList.
 *
 * (b)  Read data from UDSEntries in a UDSEntryList using
 *      UDSEntry::stringValue(uint) and UDSEntry::numberValue(uint).
 *
 * (c)  Save a UDSEntryList in a QDataStream.
 *
 * (d)  Load a UDSEntryList from a QDataStream.
 *
 * This is done for two different data sets:
 *
 * 1.   UDSEntries containing the entries which are provided by kio_file.
 *
 * 2.   UDSEntries with a larger number of "fields".
 */

// The following constants control the number of UDSEntries that are considered
// in each test, and the number of extra "fields" that are used for large UDSEntries.
const int numberOfSmallUDSEntries = 100 * 1000;
const int numberOfLargeUDSEntries = 5 * 1000;
const int extraFieldsForLargeUDSEntries = 40;


class UDSEntryBenchmark : public QObject
{
    Q_OBJECT

public:
    UDSEntryBenchmark();

private Q_SLOTS:
    void createSmallEntries();
    void createLargeEntries();
    void readFieldsFromSmallEntries();
    void readFieldsFromLargeEntries();
    void saveSmallEntries();
    void saveLargeEntries();
    void loadSmallEntries();
    void loadLargeEntries();

private:
    KIO::UDSEntryList m_smallEntries;
    KIO::UDSEntryList m_largeEntries;
    QByteArray m_savedSmallEntries;
    QByteArray m_savedLargeEntries;

    QVector<uint> m_fieldsForLargeEntries;
};

UDSEntryBenchmark::UDSEntryBenchmark()
{
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_SIZE);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_SIZE_LARGE);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_USER);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_ICON_NAME);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_GROUP);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_NAME);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_LOCAL_PATH);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_HIDDEN);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_ACCESS);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_MODIFICATION_TIME);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_ACCESS_TIME);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_CREATION_TIME);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_FILE_TYPE);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_LINK_DEST);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_URL);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_MIME_TYPE);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_GUESSED_MIME_TYPE);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_XML_PROPERTIES);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_EXTENDED_ACL);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_ACL_STRING);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_DISPLAY_NAME);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_TARGET_URL);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_DISPLAY_TYPE);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_ICON_OVERLAY_NAMES);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_COMMENT);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_DEVICE_ID);
    m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_INODE);

    for (int i = 0; i < extraFieldsForLargeUDSEntries; ++i) {
        m_fieldsForLargeEntries.append(KIO::UDSEntry::UDS_EXTRA + i);
    }
}

void UDSEntryBenchmark::createSmallEntries()
{
    m_smallEntries.clear();
    m_smallEntries.reserve(numberOfSmallUDSEntries);

    const QString user = QStringLiteral("user");
    const QString group = QStringLiteral("group");

    QVector<QString> names(numberOfSmallUDSEntries);
    for (int i = 0; i < numberOfSmallUDSEntries; ++i) {
        names[i] = QString::number(i);
    }

    QBENCHMARK_ONCE {
        for (int i = 0; i < numberOfSmallUDSEntries; ++i) {
            KIO::UDSEntry entry;
            entry.reserve(8);
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, names[i]);
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, i);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, i);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, i);
            entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, i);
            entry.fastInsert(KIO::UDSEntry::UDS_USER, user);
            entry.fastInsert(KIO::UDSEntry::UDS_GROUP, group);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, i);
            m_smallEntries.append(entry);
        }
    }

    Q_ASSERT(m_smallEntries.count() == numberOfSmallUDSEntries);
}

void UDSEntryBenchmark::createLargeEntries()
{
    m_largeEntries.clear();
    m_largeEntries.reserve(numberOfLargeUDSEntries);

    QVector<QString> names(numberOfLargeUDSEntries);
    for (int i = 0; i < numberOfLargeUDSEntries; ++i) {
        names[i] = QString::number(i);
    }

    QBENCHMARK_ONCE {
        for (int i = 0; i < numberOfLargeUDSEntries; ++i) {
            KIO::UDSEntry entry;
            entry.reserve(m_fieldsForLargeEntries.count());
            for (uint field : qAsConst(m_fieldsForLargeEntries)) {
                if (field & KIO::UDSEntry::UDS_STRING) {
                    entry.fastInsert(field, names[i]);
                } else {
                    entry.fastInsert(field, i);
                }
            }
            m_largeEntries.append(entry);
        }
    }

    Q_ASSERT(m_largeEntries.count() == numberOfLargeUDSEntries);
}

void UDSEntryBenchmark::readFieldsFromSmallEntries()
{
    // Create the entries if they do not exist yet.
    if (m_smallEntries.isEmpty()) {
        createSmallEntries();
    }

    const QString user = QStringLiteral("user");
    const QString group = QStringLiteral("group");

    QBENCHMARK {
        long long i = 0;
        long long entrySum = 0;

        for (const KIO::UDSEntry &entry : qAsConst(m_smallEntries)) {
            entrySum += entry.count();
            if (entry.stringValue(KIO::UDSEntry::UDS_NAME).toInt() == i
                && entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE) == i
                && entry.numberValue(KIO::UDSEntry::UDS_ACCESS) == i
                && entry.numberValue(KIO::UDSEntry::UDS_SIZE) == i
                &&  entry.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME) == i
                && entry.stringValue(KIO::UDSEntry::UDS_USER) == user
                && entry.stringValue(KIO::UDSEntry::UDS_GROUP) == group
                && entry.numberValue(KIO::UDSEntry::UDS_ACCESS_TIME) == i)
            {
                ++i;
            }
        }

        QCOMPARE(i, numberOfSmallUDSEntries);
        QCOMPARE(entrySum, numberOfSmallUDSEntries * 8);
    }
}

void UDSEntryBenchmark::readFieldsFromLargeEntries()
{
    // Create the entries if they do not exist yet.
    if (m_largeEntries.isEmpty()) {
        createLargeEntries();
    }

    QBENCHMARK_ONCE {
        long long i = 0;
        long long fieldSum = 0;

        for (const KIO::UDSEntry &entry : qAsConst(m_largeEntries)) {
            for (uint field : qAsConst(m_fieldsForLargeEntries)) {
                if (field & KIO::UDSEntry::UDS_STRING) {
                    if (entry.stringValue(field).toInt() == i) {
                        ++fieldSum;
                    }
                } else if (entry.numberValue(field) == i) {
                    ++fieldSum;
                }
            }
            ++i;
        }

        QCOMPARE(fieldSum, m_fieldsForLargeEntries.count() * m_largeEntries.count());
    }
}

void UDSEntryBenchmark::saveSmallEntries()
{
    // Create the entries if they do not exist yet.
    if (m_smallEntries.isEmpty()) {
        createSmallEntries();
    }

    m_savedSmallEntries.clear();

    QBENCHMARK_ONCE {
        QDataStream stream(&m_savedSmallEntries, QIODevice::WriteOnly);
        stream << m_smallEntries;
    }
}

void UDSEntryBenchmark::saveLargeEntries()
{
    // Create the entries if they do not exist yet.
    if (m_smallEntries.isEmpty()) {
        createLargeEntries();
    }

    m_savedLargeEntries.clear();

    QBENCHMARK_ONCE {
        QDataStream stream(&m_savedLargeEntries, QIODevice::WriteOnly);
        stream << m_largeEntries;
    }
}
void UDSEntryBenchmark::loadSmallEntries()
{
    // Save the entries if that has not been done yet.
    if (m_savedSmallEntries.isEmpty()) {
        saveSmallEntries();
    }

    QDataStream stream(m_savedSmallEntries);
    KIO::UDSEntryList entries;

    QBENCHMARK_ONCE {
        stream >> entries;
    }

    QCOMPARE(entries, m_smallEntries);
}

void UDSEntryBenchmark::loadLargeEntries()
{
    // Save the entries if that has not been done yet.
    if (m_savedLargeEntries.isEmpty()) {
        saveLargeEntries();
    }

    QDataStream stream(m_savedLargeEntries);
    KIO::UDSEntryList entries;

    QBENCHMARK_ONCE {
        stream >> entries;
    }

    QCOMPARE(entries, m_largeEntries);
}


QTEST_MAIN(UDSEntryBenchmark)

#include "udsentry_benchmark.moc"
