/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "testtrashdirnotify.h"

#include "../trashdirnotify.h"

#include <KDirNotify>

#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

QTEST_GUILESS_MAIN(TestTrashDirNotify)

QString TestTrashDirNotify::filesDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash/files");
}

void TestTrashDirNotify::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestTrashDirNotify::init()
{
    m_notifier = new TrashDirNotify;
    QVERIFY(QDir(filesDir()).exists());
}

void TestTrashDirNotify::cleanup()
{
    delete m_notifier;
    m_notifier = nullptr;

    const QString trashDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash");
    QDir(trashDir).removeRecursively();
}

void TestTrashDirNotify::testFileCreatedInPhysicalTrash()
{
    org::kde::KDirNotify listener(QString(), QString(), QDBusConnection::sessionBus());
    QSignalSpy filesChangedSpy(&listener, &org::kde::KDirNotify::FilesChanged);

    QFile file(filesDir() + QStringLiteral("/externally-added-file"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QVERIFY(filesChangedSpy.wait(5000));
    QCOMPARE(filesChangedSpy.takeFirst().at(0).toStringList(), QStringList{QStringLiteral("trash:/")});
}

void TestTrashDirNotify::testFileRemovedFromPhysicalTrash()
{
    const QString filePath = filesDir() + QStringLiteral("/externally-removed-file");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QTest::qWait(200);

    org::kde::KDirNotify listener(QString(), QString(), QDBusConnection::sessionBus());
    QSignalSpy filesChangedSpy(&listener, &org::kde::KDirNotify::FilesChanged);

    QVERIFY(QFile::remove(filePath));

    QVERIFY(filesChangedSpy.wait(5000));
    QCOMPARE(filesChangedSpy.takeFirst().at(0).toStringList(), QStringList{QStringLiteral("trash:/")});
}

#include "moc_testtrashdirnotify.cpp"
