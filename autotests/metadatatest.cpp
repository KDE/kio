/*
    SPDX-FileCopyrightText: 2025 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "metadatatest.h"

#include <KIO/MetaData>

#include <QTest>

using namespace Qt::StringLiterals;

void MetaDataTest::testToVariant()
{
    const QMap<QString, QString> data = {
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

QTEST_GUILESS_MAIN(MetaDataTest)

#include "moc_metadatatest.cpp"
