/*
    SPDX-FileCopyrightText: 2025 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "metadatatest.h"

#include <KIO/MetaData>

#include <QTest>

using namespace Qt::StringLiterals;

// typedef needed for passing _data() method
using TestDataMap = QMap<QString, QString>;

void MetaDataTest::testToVariant()
{
    const TestDataMap data = {
        {u"keyA"_s, u"valueOne"_s},
        {u"keyB"_s, u"valueTwo"_s},
    };

    // create
    KIO::MetaData metaData;
    for (const auto &[key, value] : data.asKeyValueRange()) {
        metaData.insert(key, value);
    }
    QCOMPARE(metaData.size(), data.size());

    // call to be tested method
    const QVariant variant = metaData.toVariant();

    // inspect properties
    QCOMPARE(variant.typeId(), QMetaType::QVariantMap);

    const QVariantMap variantMap = variant.toMap();
    QCOMPARE(variantMap.size(), data.size());

    auto it = variantMap.begin();
    for (const auto &[expectedKey, expectedValue] : data.asKeyValueRange()) {
        const QString &mapKey = it.key();
        QCOMPARE(mapKey, expectedKey);

        const QVariant &mapValue = it.value();
        QCOMPARE(mapValue.typeId(), QMetaType::QString);
        QCOMPARE(mapValue.toString(), expectedValue);

        ++it;
    }
}

// typedef needed for passing _data() method
// type used as arg type for MetaData::operator+=()
using VariantMap = QMap<QString, QVariant>;

void MetaDataTest::testOperatorPlusEqualVariantMap_data()
{
    QTest::addColumn<TestDataMap>("originalData");
    QTest::addColumn<VariantMap>("operandData");
    QTest::addColumn<TestDataMap>("expectedData");

    const QString keyA = u"keyA"_s;
    const QString keyB = u"keyB"_s;
    const QString valueOne = u"valueOne"_s;
    const QString valueTwo = u"valueTwo"_s;
    const QString digit3 = u"3"_s;

    /* clang-format off */
    QTest::newRow("empty+empty")
        << TestDataMap()
        << VariantMap()
        << TestDataMap();

    QTest::newRow("A+B")
        << TestDataMap{{keyA, valueOne}}
        << VariantMap{{keyB, QVariant(valueTwo)}}
        << TestDataMap{{keyA, valueOne}, {keyB, valueTwo}};

    QTest::newRow("A+3")
        << TestDataMap{{keyA, valueOne}}
        << VariantMap{{keyB, QVariant(3)}}
        << TestDataMap{{keyA, valueOne}, {keyB, digit3}};

    QTest::newRow("A+A")
        << TestDataMap{{keyA, valueOne}}
        << VariantMap{{keyA, QVariant(valueOne)}}
        << TestDataMap{{keyA, valueOne}};

    QTest::newRow("A+otherA")
        << TestDataMap{{keyA, valueOne}}
        << VariantMap{{keyA, QVariant(valueTwo)}}
        << TestDataMap{{keyA, valueTwo}};

    QTest::newRow("A+otherTypeA")
        << TestDataMap{{keyA, valueOne}}
        << VariantMap{{keyA, QVariant(3)}}
        << TestDataMap{{keyA, digit3}};
    /* clang-format on */
}

void MetaDataTest::testOperatorPlusEqualVariantMap()
{
    QFETCH(const TestDataMap, originalData);
    QFETCH(const VariantMap, operandData);
    QFETCH(const TestDataMap, expectedData);

    // create
    KIO::MetaData metaData;
    for (const auto &[key, value] : originalData.asKeyValueRange()) {
        metaData.insert(key, value);
    }
    QCOMPARE(metaData.size(), originalData.size());

    // call to be tested method
    metaData += operandData;

    // check result
    QCOMPARE(metaData.size(), expectedData.size());

    auto it = metaData.begin();
    for (const auto &[expectedKey, expectedValue] : expectedData.asKeyValueRange()) {
        const QString &metaDataKey = it.key();
        QCOMPARE(metaDataKey, expectedKey);

        const QString &metaDataValue = it.value();
        QCOMPARE(metaDataValue, expectedValue);

        ++it;
    }
}

QTEST_GUILESS_MAIN(MetaDataTest)

#include "moc_metadatatest.cpp"
