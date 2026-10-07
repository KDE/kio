/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KSambaShare>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// KSambaShare loads once per process, so this runs apart from ksambasharetest, with a testparm
// that fails to start and a net that answers late.
class KSambaShareLoadTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testGetterWaitsForTheLoad();

private:
    bool writeScript(const QString &name, const QByteArray &content);

    QTemporaryDir m_binDir;
};

bool KSambaShareLoadTest::writeScript(const QString &name, const QByteArray &content)
{
    QFile file(m_binDir.filePath(name));
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size()) {
        return false;
    }
    return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

void KSambaShareLoadTest::initTestCase()
{
    QVERIFY(m_binDir.isValid());
    // Executable, but its interpreter does not exist, so QProcess reports FailedToStart.
    QVERIFY(writeScript(QStringLiteral("testparm"), QByteArrayLiteral("#!/kio-ksambashareloadtest-no-interpreter\n")));
    const QByteArray share = "[loadtest]\\npath=" + QDir::tempPath().toUtf8() + "\\ncomment=\\nusershare_acl=Everyone:R,\\nguest_ok=n\\n";
    QVERIFY(writeScript(QStringLiteral("net"), QByteArray("#!/bin/sh\nsleep 0.3\nprintf '" + share + "'\n")));
    qputenv("PATH", QByteArray(m_binDir.path().toUtf8() + ':' + qgetenv("PATH")));
}

void KSambaShareLoadTest::testGetterWaitsForTheLoad()
{
    QSignalSpy changedSpy(KSambaShare::instance(), &KSambaShare::changed);

    // testparm fails to start, and the load goes on with net.
    QCOMPARE(KSambaShare::instance()->shareNames(), QStringList{QStringLiteral("loadtest")});
    QVERIFY(KSambaShare::instance()->isDirectoryShared(QDir::tempPath()));

    // changed() is not emitted from inside the getter.
    QCOMPARE(changedSpy.count(), 0);
    QTRY_COMPARE(changedSpy.count(), 1);
}

QTEST_GUILESS_MAIN(KSambaShareLoadTest)

#include "ksambashareloadtest.moc"
