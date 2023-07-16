/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class PutTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void PutTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QByteArray>("inputData");

    QTest::addRow("put") << "http://localhost:5000/put/bla" << QByteArray("<p>Hello, World!</p>");
}

void PutTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QByteArray, inputData);

    auto *job = KIO::put(QUrl(url), -1);
    job->addMetaData("content-type", "text/html");

    int dataReqCounter = 0;
    connect(job, &KIO::TransferJob::dataReq, this, [inputData, &dataReqCounter](KJob * /*job*/, QByteArray &data) {
        if (dataReqCounter < 3) {
            data = inputData;
        }
        dataReqCounter++;
    });

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());
    QCOMPARE(dataReqCounter, 4);

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, inputData + inputData + inputData);

    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(PutTest)

#include "puttest.moc"
