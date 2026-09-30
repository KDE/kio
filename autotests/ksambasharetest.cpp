/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "ksambasharetest.h"

#include <KSambaShare>
#include <KSambaShareData>

#include <QSignalSpy>
#include <QTest>

QTEST_MAIN(KSambaShareTest)

Q_DECLARE_METATYPE(KSambaShareData::UserShareError)

void KSambaShareTest::initTestCase()
{
    qRegisterMetaType<KSambaShareData::UserShareError>();
}

void KSambaShareTest::testInitialLoadEmitsChanged()
{
    // instance() starts the load in a worker thread and returns before it finishes,
    // so the result is announced with changed() once it has been applied. Nothing has
    // run the event loop yet at this point, so the signal cannot have been missed.
    QSignalSpy changedSpy(KSambaShare::instance(), &KSambaShare::changed);
    QCOMPARE(changedSpy.count(), 0);

    QVERIFY(changedSpy.wait(10000));
    QCOMPARE(changedSpy.count(), 1);
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

void KSambaShareTest::testSharedDirectoriesAreShared()
{
    // sharedDirectories() waits for the load, isDirectoryShared() answers from what has
    // already been applied. Once the load is in, the two agree.
    const QStringList dirs = KSambaShare::instance()->sharedDirectories();
    for (const QString &dir : dirs) {
        QVERIFY(KSambaShare::instance()->isDirectoryShared(dir));
    }

    QVERIFY(!KSambaShare::instance()->isDirectoryShared(QStringLiteral("/kio-ksambasharetest-not-a-share")));
}

#include "moc_ksambasharetest.cpp"
