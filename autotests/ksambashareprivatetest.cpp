/*
    SPDX-FileCopyrightText: 2018 Stefan Brüns <stefan.bruens@rwth-aachen.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "ksambashareprivatetest.h"
#include "ksambashare_p.h"

#include <KSambaShareData>

#include <QTest>

QTEST_MAIN(KSambaSharePrivateTest)

void KSambaSharePrivateTest::initTestCase()
{
}

void KSambaSharePrivateTest::testParser()
{
    QFETCH(QByteArray, usershareData);
    QFETCH(bool, valid);
    QFETCH(QString, share);
    QFETCH(QString, path);

    auto shares = KSambaSharePrivate::parse(usershareData);

    if (valid) {
        QCOMPARE(shares.size(), 1);
        QCOMPARE(shares.first().name(), share);
        QCOMPARE(shares.first().path(), path);
        QCOMPARE(shares.first().path(), path);
    } else {
        QCOMPARE(shares.size(), 0);
    }
}

void KSambaSharePrivateTest::testParser_data()
{
    QTest::addColumn<QByteArray>("usershareData");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("share");
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("comment");

    QTest::newRow("Valid share") << QByteArrayLiteral("[share]\npath=/some/path\ncomment=\nusershare_acl=Everyone:R,\nguest_ok=y")
                                 << true << "share" << "/some/path" << "";
    QTest::newRow("Valid share with slash") << QByteArrayLiteral("[share]\npath=/some/path/\ncomment=\nusershare_acl=Everyone:R,\nguest_ok=y")
                                 << true << "share" << "/some/path" << "";
    QTest::newRow("Valid share with comment") << QByteArrayLiteral("[share]\npath=/some/path\ncomment=Comment\nusershare_acl=Everyone:R,\nguest_ok=y")
                                 << true << "share" << "/some/path" << "Comment";
}
