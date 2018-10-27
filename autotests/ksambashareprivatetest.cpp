/*
 * Copyright (c) 2018 Stefan Brüns <stefan.bruens@rwth-aachen.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
