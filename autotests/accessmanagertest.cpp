/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>
#include <accessmanager.h>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QProcess>
#include <QStandardPaths>
#include <QBuffer>

/**
 * Unit test for AccessManager
 */
class AccessManagerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        qputenv("KDE_FORK_SLAVES", "yes"); // To avoid a runtime dependency on klauncher
        qputenv("KIOSLAVE_ENABLE_TESTMODE", "1"); // ensure the ioslaves call QStandardPaths::setTestModeEnabled too
        QStandardPaths::setTestModeEnabled(true);
    }

    void testGet()
    {
        const QString aFile = QFINDTESTDATA("accessmanagertest.cpp");
        QNetworkReply *reply = manager()->get(QNetworkRequest(QUrl::fromLocalFile(aFile)));
        QSignalSpy spy(reply, &QNetworkReply::finished);
        QVERIFY(spy.wait());

        QFile f(aFile);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), reply->readAll());
    }

    void testPut()
    {
        const QString aDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QVERIFY(QDir::temp().mkpath(aDir));
        const QString aFile = aDir + QStringLiteral("/accessmanagertest-data");
        const QByteArray content = "We love free software!";
        QBuffer buffer;
        buffer.setData(content);
        QVERIFY(buffer.open(QIODevice::ReadOnly));

        QFile::remove(aFile);

        QNetworkReply *reply = manager()->put(QNetworkRequest(QUrl::fromLocalFile(aFile)), &buffer);
        QSignalSpy spy(reply, &QNetworkReply::finished);
        QVERIFY(reply->isRunning());
        QVERIFY(spy.wait());

        QVERIFY(QFile::exists(aFile));
        QFile f(aFile);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), content);

        QFile::remove(aFile);
    }

    void testPutSequential()
    {
        const QString aDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QVERIFY(QDir::temp().mkpath(aDir));
        const QString aFile = aDir + QStringLiteral("/accessmanagertest-data2");
        const QString putDataContents = "We love free software! " + QString(24000, 'c');
        QProcess process;
        process.start(QStringLiteral("echo"), QStringList{putDataContents});

        QFile::remove(aFile);

        QNetworkReply *reply = manager()->put(QNetworkRequest(QUrl::fromLocalFile(aFile)), &process);
        QSignalSpy spy(reply, &QNetworkReply::finished);
        QVERIFY(spy.wait());
        QVERIFY(QFile::exists(aFile));

        QFile f(aFile);
        QVERIFY(f.open(QIODevice::ReadOnly));

        QByteArray cts = f.readAll();
        cts.chop(1); //we remove the eof
        QCOMPARE(QString::fromUtf8(cts).size(), putDataContents.size());
        QCOMPARE(QString::fromUtf8(cts), putDataContents);

        QFile::remove(aFile);
    }

private:
    /**
     * we want to run the tests both on QNAM and KIO::AccessManager
     * to make sure they behave the same way.
     */
    QNetworkAccessManager *manager()
    {
        static QNetworkAccessManager *ret = nullptr;
        if (!ret) {
#ifdef USE_QNAM
            ret = new QNetworkAccessManager(this);
#else
            ret = new KIO::AccessManager(this);
#endif
        }
        return ret;
    }
};

QTEST_MAIN(AccessManagerTest)

#include "accessmanagertest.moc"
