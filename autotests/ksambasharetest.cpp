/*
 * Copyright (c) 2018 Kai Uwe Broulik <kde@broulik.de>
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

#include "ksambasharetest.h"

#include <KSambaShare>
#include <KSambaShareData>

#include <QTest>

QTEST_MAIN(KSambaShareTest)

Q_DECLARE_METATYPE(KSambaShareData::UserShareError)

void KSambaShareTest::initTestCase()
{
    qRegisterMetaType<KSambaShareData::UserShareError>();
}

void KSambaShareTest::testAcl()
{
    QFETCH(QString, acl);
    QFETCH(KSambaShareData::UserShareError, result);

    KSambaShareData data;

    QCOMPARE(data.setAcl(acl), result);
}

void KSambaShareTest::testAcl_data()
{
    QTest::addColumn<QString>("acl");
    QTest::addColumn<KSambaShareData::UserShareError>("result");

    QTest::newRow("one entry") << QStringLiteral("Everyone:r") << KSambaShareData::UserShareAclOk;
    QTest::newRow("one entry, trailing comma") << QStringLiteral("Everyone:r,") << KSambaShareData::UserShareAclOk;

    QTest::newRow("one entry with hostname") << QStringLiteral("Host\\Someone:r") << KSambaShareData::UserShareAclOk;

    QTest::newRow("space in hostname") << QStringLiteral("Everyone:r,Unix User\\Someone:f,") << KSambaShareData::UserShareAclOk;

    QTest::newRow("garbage") << QStringLiteral("Garbage") << KSambaShareData::UserShareAclInvalid;
}

void KSambaShareTest::testOwnAcl()
{
    const auto shares = KSambaShare::instance()->shareNames();

    for (const QString &share : shares) {
        KSambaShareData shareData = KSambaShare::instance()->getShareByName(share);

        // KSambaShare reads acl from net usershare info's "usershare_acl" field with no validation
        QCOMPARE(shareData.setAcl(shareData.acl()), KSambaShareData::UserShareAclOk);
    }
}
