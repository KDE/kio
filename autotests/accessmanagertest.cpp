/* This file is part of the KDE libraries
    Copyright (c) 2015 Aleix Pol Gonzalez <aleixpol@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtTest/QtTest>
#include <accessmanager.h>
#include <QNetworkReply>

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
        QNetworkReply* reply = manager()->get(QNetworkRequest(QUrl::fromLocalFile(aFile)));
        QSignalSpy spy(reply, SIGNAL(finished()));
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

        QNetworkReply* reply = manager()->put(QNetworkRequest(QUrl::fromLocalFile(aFile)), &buffer);
        QSignalSpy spy(reply, SIGNAL(finished()));
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
        process.start("echo", QStringList() << putDataContents);

        QFile::remove(aFile);

        QNetworkReply* reply = manager()->put(QNetworkRequest(QUrl::fromLocalFile(aFile)), &process);
        QSignalSpy spy(reply, SIGNAL(finished()));
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
    QNetworkAccessManager* manager()
    {
        static QNetworkAccessManager* ret = Q_NULLPTR;
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
