/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class ErrorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();

    void testPut();
    void testPut_data();
};

void ErrorTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("expectedError");

    QTest::addRow("noerror") << "http://localhost:5000/error/no" << (int)KJob::NoError;
    QTest::addRow("404") << "http://localhost:5000/error/404" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("400") << "http://localhost:5000/error/400" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("403") << "http://localhost:5000/error/403" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("451") << "http://localhost:5000/error/451" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("500") << "http://localhost:5000/error/500" << (int)KIO::ERR_INTERNAL_SERVER;
    QTest::addRow("502") << "http://localhost:5000/error/502" << (int)KIO::ERR_INTERNAL_SERVER;
    QTest::addRow("507") << "http://localhost:5000/error/507" << (int)KIO::ERR_INTERNAL_SERVER;
}

void ErrorTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(int, expectedError);

    auto *job = KIO::get(QUrl(url));

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());

    QCOMPARE(job->error(), expectedError);
}

void ErrorTest::testPut_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<int>("expectedError");

    QTest::addRow("noerror") << "http://localhost:5000/error/no" << (int)KJob::NoError;
    QTest::addRow("404") << "http://localhost:5000/error/404" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("400") << "http://localhost:5000/error/400" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("403") << "http://localhost:5000/error/403" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("405") << "http://localhost:5000/error/405" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("451") << "http://localhost:5000/error/451" << (int)KIO::ERR_DOES_NOT_EXIST;
    QTest::addRow("500") << "http://localhost:5000/error/500" << (int)KIO::ERR_INTERNAL_SERVER;
    QTest::addRow("502") << "http://localhost:5000/error/502" << (int)KIO::ERR_INTERNAL_SERVER;
    QTest::addRow("507") << "http://localhost:5000/error/507" << (int)KIO::ERR_INTERNAL_SERVER;
}

void ErrorTest::testPut()
{
    QFETCH(QString, url);
    QFETCH(int, expectedError);

    auto *job = KIO::put(QUrl(url), -1);

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());

    QCOMPARE(job->error(), expectedError);
}

QTEST_GUILESS_MAIN(ErrorTest)

#include "errortest.moc"
