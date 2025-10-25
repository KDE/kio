/*
    SPDX-FileCopyrightText: 2025 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_METADATATEST_H
#define KIO_METADATATEST_H

#include <QObject>

class MetaDataTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testToVariant();

    void testOperatorPlusEqualVariantMap_data();
    void testOperatorPlusEqualVariantMap();
};

#endif
