/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "privilegejobtest.h"

#include <QTest>

#include <KIO/ChmodJob>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KIO/MkpathJob>
#include <KIO/SimpleJob>
#include <KIO/TransferJob>

#include "kiotesthelper.h"

QTEST_MAIN(PrivilegeJobTest)

void PrivilegeJobTest::initTestCase()
{
    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");

    cleanupTestCase();
    homeTmpDir();
    m_testFilePath = homeTmpDir() + "testfile";
    createTestFile(m_testFilePath);
    QVERIFY(QFile::exists(m_testFilePath));
    QVERIFY(QFile::setPermissions(homeTmpDir(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
}

void PrivilegeJobTest::cleanupTestCase()
{
    QFile::setPermissions(homeTmpDir(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QDir(homeTmpDir()).removeRecursively();
}

void PrivilegeJobTest::privilegeChmod()
{
    KFileItem item(QUrl::fromLocalFile(m_testFilePath));
    const mode_t origPerm = item.permissions();
    mode_t newPerm = origPerm ^ S_IWGRP;
    QVERIFY(newPerm != origPerm);
    // Remove search permission
    QVERIFY(QFile::setPermissions(homeTmpDir(), QFileDevice::ReadOwner));
    KFileItemList items; items << item;
    KIO::Job *job = KIO::chmod(items, newPerm, S_IWGRP, QString(), QString(), false, KIO::HideProgressInfo);
    job->addMetaData("UnitTesting", "true");
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->queryMetaData("TestData"), QLatin1String("PrivilegeOperationAllowed"));
    // Bring it back
    QVERIFY(QFile::setPermissions(homeTmpDir(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
}

void PrivilegeJobTest::privilegeCopy()
{
    const QUrl src = QUrl::fromLocalFile(m_testFilePath);
    const QUrl dest = QUrl::fromLocalFile(homeTmpDir() + "newtestfile");
    KIO::CopyJob *job = KIO::copy(src, dest, KIO::HideProgressInfo);
    job->addMetaData("UnitTesting", "true");
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->queryMetaData("TestData"), QStringLiteral("PrivilegeOperationAllowed"));
}

void PrivilegeJobTest::privilegeDelete()
{
    const QUrl url = QUrl::fromLocalFile(m_testFilePath);
    KIO::DeleteJob *job = KIO::del(url, KIO::HideProgressInfo);
    job->addMetaData("UnitTesting", "true");
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->queryMetaData("TestData"), QStringLiteral("PrivilegeOperationAllowed"));
}

void PrivilegeJobTest::privilegeMkpath()
{
    const QUrl dirUrl = QUrl::fromLocalFile(homeTmpDir() + "testdir");
    KIO::MkpathJob *job = KIO::mkpath(dirUrl, QUrl(), KIO::HideProgressInfo);
    job->addMetaData("UnitTesting", "true");
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->queryMetaData("TestData"), QStringLiteral("PrivilegeOperationAllowed"));
}

void PrivilegeJobTest::privilegePut()
{
    const QUrl url = QUrl::fromLocalFile(homeTmpDir() + "putfile");
    KIO::TransferJob *job = KIO::put(url, -1, KIO::HideProgressInfo);
    job->addMetaData("UnitTesting", "true");
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->queryMetaData("TestData"), QStringLiteral("PrivilegeOperationAllowed"));
}

void PrivilegeJobTest::privilegeRename()
{
    const QUrl src = QUrl::fromLocalFile(homeTmpDir() + "testfile");
    const QUrl dest = QUrl::fromLocalFile(homeTmpDir() + "newtestfile");
    KIO::SimpleJob *job = KIO::rename(src, dest, KIO::HideProgressInfo);
    job->addMetaData("UnitTesting", "true");
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->queryMetaData("TestData"), QStringLiteral("PrivilegeOperationAllowed"));
}

void PrivilegeJobTest::privileSymlink()
{
    const QString target = homeTmpDir() + "testfile";
    const QUrl dest = QUrl::fromLocalFile(homeTmpDir() + "symlink");
    KIO::SimpleJob *job = KIO::symlink(target, dest, KIO::HideProgressInfo);
    job->addMetaData("UnitTesting", "true");
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->queryMetaData("TestData"), QStringLiteral("PrivilegeOperationAllowed"));
}
