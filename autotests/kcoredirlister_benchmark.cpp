/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2018 Jaime Torres <jtamate@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <kfileitem.h>

#include <QList>
#include <QHash>
#include <QMap>

#include <algorithm>
#include <random>

// BEGIN Global variables
const QString fileNameArg = QLatin1String("/home/user/Folder1/SubFolder2/a%1.txt");
// to check with 10, 100, 1000, ... KFileItem
const int maxPowerOfTen=3;
// To use the same random list of names and url for all the tests
QVector<int> randInt[maxPowerOfTen];
// The same list of random integers for all the tests
std::default_random_engine generator;

// END Global variables

/*
   This is to compare the old list API vs QMap API vs QHash API vs sorted list API
   in terms of performance for KcoreDirLister list of items.
   This benchmark assumes that KFileItem has the < operators.
*/
class kcoreDirListerEntryBenchmark : public QObject
{
    Q_OBJECT
public:
    kcoreDirListerEntryBenchmark() {
        // Fill randInt[i] with random numbers from 0 to (10^(i+1))-1
        for (int i=0; i < maxPowerOfTen; ++i) {
            std::uniform_int_distribution<int> distribution(0,pow(10,i+1)-1);

            // Fill the vector with consecutive numbers
            randInt[i].reserve(pow(10,i+1));
            for (int j=0; j < pow(10,i+1); ++j) {
                randInt[i].append(j);
            }
            // And now scramble them a little bit
            for (int j=0; j < pow(10,i+1); ++j) {
                int rd1 = distribution(generator);
                int rd2 = distribution(generator);
                int swap = randInt[i].at(rd1);
                randInt[i].replace(rd1, randInt[i].at(rd2));
                randInt[i].replace(rd2, swap);
            }
            // qDebug() << randInt[i];
        }
    }
private Q_SLOTS:
    void testCreateFiles_List_data();
    void testCreateFiles_List();
    void testFindByNameFiles_List_data();
    void testFindByNameFiles_List();
    void testFindByUrlFiles_List_data();
    void testFindByUrlFiles_List();
    void testFindByUrlAllFiles_List_data();
    void testFindByUrlAllFiles_List();

    void testCreateFiles_Map_data();
    void testCreateFiles_Map();
    void testFindByNameFiles_Map_data();
    void testFindByNameFiles_Map();
    void testFindByUrlFiles_Map_data();
    void testFindByUrlFiles_Map();
    void testFindByUrlAllFiles_Map_data();
    void testFindByUrlAllFiles_Map();

    void testCreateFiles_Hash_data();
    void testCreateFiles_Hash();
    void testFindByNameFiles_Hash_data();
    void testFindByNameFiles_Hash();
    void testFindByUrlFiles_Hash_data();
    void testFindByUrlFiles_Hash();
    void testFindByUrlAllFiles_Hash_data();
    void testFindByUrlAllFiles_Hash();

    void testCreateFiles_Binary_data();
    void testCreateFiles_Binary();
    void testFindByNameFiles_Binary_data();
    void testFindByNameFiles_Binary();
    void testFindByUrlFiles_Binary_data();
    void testFindByUrlFiles_Binary();
    void testFindByUrlAllFiles_Binary_data();
    void testFindByUrlAllFiles_Binary();
};


//BEGIN Implementations

//BEGIN List
// List Implementation (without binary search)
class ListImplementation
{
public:
    QList<KFileItem> lstItems;
public:
    void reserve(int size)
    {
        lstItems.reserve(size);
    }

    // This search must be fast also
    KFileItem findByName(const QString &fileName) const
    {
        const auto end = lstItems.cend();
        for (auto it = lstItems.cbegin() ; it != end; ++it) {
            if ((*it).name() == fileName) {
                return *it;
            }
        }
        return KFileItem();
    }

    // simulation of the search by Url in an existing lister (the slowest path)
    KFileItem findByUrl(const QUrl &_u) const
    {
        QUrl url(_u);
        url = url.adjusted(QUrl::StripTrailingSlash);
        const auto end = lstItems.cend();
        for (auto it = lstItems.cbegin(); it != end; ++it) {
            if ((*it).url() == url) {
                return *it;
            }
        }
        return KFileItem();
    }

    void clear()
    {
        lstItems.clear();
    }

    void insert(int powerOfTen)
    {
        for (int x = 0; x < pow(10, powerOfTen+1); ++x) {
            QUrl u = QUrl::fromLocalFile(fileNameArg.arg(randInt[powerOfTen].at(x) )).adjusted(QUrl::StripTrailingSlash);

            KFileItem kfi (u, QStringLiteral("text/text"));
            lstItems.append(kfi);
        }
    }
};
//END List
//BEGIN QMap
// Proposed Implementation using QMap
class QMapImplementation
{
public:
    void reserve(int size)
    {
        Q_UNUSED(size);
    }

    KFileItem findByName(const QString &fileName) const
    {
        const auto itend = lstItems.cend();
        for (auto it = lstItems.cbegin(); it != itend; ++it) {
            if ((*it).name() == fileName) {
                return *it;
            }
        }
        return KFileItem();
    }

    // simulation of the search by Url in an existing lister (the slowest path)
    KFileItem findByUrl(const QUrl &_u) const
    {
        QUrl url(_u);
        url = url.adjusted(QUrl::StripTrailingSlash);

        auto it = lstItems.find(url);
        if (it != lstItems.end()) {
            return *it;
        }
        return KFileItem();
    }

    void clear()
    {
        lstItems.clear();
    }

    void insert(int powerOfTen)
    {
        for (int x = 0; x < pow(10, powerOfTen+1); ++x) {
            QUrl u = QUrl::fromLocalFile(fileNameArg.arg(randInt[powerOfTen].at(x) )).adjusted(QUrl::StripTrailingSlash);

            KFileItem kfi(u, QStringLiteral("text/text"));

            lstItems.insert(u, kfi);
        }
    }
public:
    QMap<QUrl, KFileItem> lstItems;
};
//END QMap
//BEGIN QHash
// Proposed Implementation using QHash
class QHashImplementation
{
public:
    void reserve(int size)
    {
        lstItems.reserve(size);
    }
    KFileItem findByName(const QString &fileName) const
    {
        const auto itend = lstItems.cend();
        for (auto it = lstItems.cbegin(); it != itend; ++it) {
            if ((*it).name() == fileName) {
                return *it;
            }
        }
        return KFileItem();
    }

    // simulation of the search by Url in an existing lister (the slowest path)
    KFileItem findByUrl(const QUrl &_u) const
    {
        QUrl url(_u);
        url = url.adjusted(QUrl::StripTrailingSlash);

        auto it = lstItems.find(url);
        if (it != lstItems.end()) {
            return *it;
        }
        return KFileItem();
    }

    void clear()
    {
        lstItems.clear();
    }

    void insert(int powerOfTen)
    {
        for (int x = 0; x < pow(10, powerOfTen+1); ++x) {
            QUrl u = QUrl::fromLocalFile(fileNameArg.arg(randInt[powerOfTen].at(x) )).adjusted(QUrl::StripTrailingSlash);

            KFileItem kfi(u, QStringLiteral("text/text"));

            lstItems.insert(u, kfi);
        }
    }
public:
    QHash<QUrl, KFileItem> lstItems;
};
//END QHash
//BEGIN BinaryList
// Proposed Implementation using QList with ordered insert and binary search
class BinaryListImplementation
{
public:
    QList<KFileItem> lstItems;
public:
    void reserve(int size)
    {
        lstItems.reserve(size);
    }
    KFileItem findByName(const QString &fileName) const
    {
        const auto itend = lstItems.cend();
        for (auto it = lstItems.cbegin(); it != itend; ++it) {
            if ((*it).name() == fileName) {
                return *it;
            }
        }
        return KFileItem();
    }

    // simulation of the search by Url in an existing lister (the slowest path)
    KFileItem findByUrl(const QUrl &_u) const
    {
        QUrl url(_u);
        url = url.adjusted(QUrl::StripTrailingSlash);

        auto it = std::lower_bound(lstItems.cbegin(), lstItems.cend(), url);
        if (it != lstItems.cend() && (*it).url() == url) {
                return *it;
        }
        return KFileItem();
    }

    void clear()
    {
        lstItems.clear();
    }

    // Add files in random order from the randInt vector
    void insert(int powerOfTen)
    {
        for (int x = 0; x < pow(10, powerOfTen+1); ++x) {
            QUrl u = QUrl::fromLocalFile(fileNameArg.arg(randInt[powerOfTen].at(x) )).adjusted(QUrl::StripTrailingSlash);

            KFileItem kfi(u, QStringLiteral("text/text"));
            auto it = std::lower_bound(lstItems.begin(), lstItems.end(), u);
            lstItems.insert(it, kfi);
        }
    }
};
//END BinaryList
//END Implementations

//BEGIN templates

template <class T> void fillNumberOfFiles() {
    QTest::addColumn<int>("numberOfFiles");
    for (int i=0; i < maxPowerOfTen; ++i) {
        // it shows numberOfFiles: 10, 100 or 1000 but the data is the power of ten
        QTest::newRow( QStringLiteral("%1").arg(pow(10, i+1)).toLatin1() ) << i;
    }
}

template <class T> void createFiles(int powerOfTen)
{
    T data;
    const int numberOfFiles = pow(10, powerOfTen+1);
    data.reserve(numberOfFiles);
    QBENCHMARK {
        data.clear();
        data.insert(powerOfTen);
    }
    QCOMPARE(data.lstItems.size(), numberOfFiles);
}

template <class T> void findByName(int powerOfTen)
{
    T data;
    data.clear();
    data.reserve(pow(10, powerOfTen+1));
    data.insert(powerOfTen);

    QBENCHMARK {
        for (int i=0; i<powerOfTen; i++) {
            QString randName = QStringLiteral("a%1.txt").arg(pow(10,i));
            KFileItem item = data.findByName(randName);
            // QCOMPARE(item.name(), randName);
        }
    }
    QVERIFY(data.findByName(QLatin1String("b1.txt")).isNull());
}

template <class T> void findByUrl(int powerOfTen)
{
    T data;
    data.clear();
    data.reserve(pow(10, powerOfTen+1));
    data.insert(powerOfTen);
    QBENCHMARK {
        for (int i=0; i<powerOfTen; i++) {
            QUrl randUrl = QUrl::fromLocalFile(fileNameArg.arg(pow(10, i)));
            KFileItem item = data.findByUrl(randUrl);
            // QCOMPARE(item.url(), randUrl);
        }
    }
    QVERIFY(data.findByUrl(QUrl::fromLocalFile(QLatin1String("/home/user/Folder1/SubFolder1/b1.txt"))).isNull());
}

template <class T> void findByUrlAll(int powerOfTen)
{
    T data;
    data.clear();
    data.reserve(pow(10, powerOfTen+1));
    data.insert(powerOfTen);
    QBENCHMARK {
        for (int i=0; i<pow(10, powerOfTen+1); i++) {
            QUrl u = QUrl::fromLocalFile(fileNameArg.arg(i)).adjusted(QUrl::StripTrailingSlash);
            data.findByUrl(u);
        }
    }
}

//END templates

//BEGIN tests
void kcoreDirListerEntryBenchmark::testCreateFiles_List_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testCreateFiles_List()
{
    QFETCH(int, numberOfFiles);
    createFiles<ListImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_List_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_List()
{
    QFETCH(int, numberOfFiles);
    findByName<ListImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_List_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_List()
{
    QFETCH(int, numberOfFiles);
    findByUrl<ListImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_List_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_List()
{
    QFETCH(int, numberOfFiles);
    findByUrlAll<ListImplementation>(numberOfFiles);
}

void kcoreDirListerEntryBenchmark::testCreateFiles_Map_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testCreateFiles_Map()
{
    QFETCH(int, numberOfFiles);
    createFiles<QMapImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_Map_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_Map()
{
    QFETCH(int, numberOfFiles);
    findByName<QMapImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_Map_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_Map()
{
    QFETCH(int, numberOfFiles);
    findByUrl<QMapImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_Map_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_Map()
{
    QFETCH(int, numberOfFiles);
    findByUrlAll<QMapImplementation>(numberOfFiles);
}

void kcoreDirListerEntryBenchmark::testCreateFiles_Hash_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testCreateFiles_Hash()
{
    QFETCH(int, numberOfFiles);
    createFiles<QHashImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_Hash_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_Hash()
{
    QFETCH(int, numberOfFiles);
    findByName<QHashImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_Hash_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_Hash()
{
    QFETCH(int, numberOfFiles);
    findByUrl<QHashImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_Hash_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_Hash()
{
    QFETCH(int, numberOfFiles);
    findByUrlAll<QHashImplementation>(numberOfFiles);
}

void kcoreDirListerEntryBenchmark::testCreateFiles_Binary_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testCreateFiles_Binary()
{
    QFETCH(int, numberOfFiles);
    createFiles<BinaryListImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_Binary_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByNameFiles_Binary()
{
    QFETCH(int, numberOfFiles);
    findByName<BinaryListImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_Binary_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlFiles_Binary()
{
    QFETCH(int, numberOfFiles);
    findByUrl<BinaryListImplementation>(numberOfFiles);
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_Binary_data()
{
    fillNumberOfFiles<ListImplementation>();
}
void kcoreDirListerEntryBenchmark::testFindByUrlAllFiles_Binary()
{
    QFETCH(int, numberOfFiles);
    findByUrlAll<BinaryListImplementation>(numberOfFiles);
}

//END tests

QTEST_MAIN(kcoreDirListerEntryBenchmark)

#include "kcoredirlister_benchmark.moc"
