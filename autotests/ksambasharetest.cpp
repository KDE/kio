/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
