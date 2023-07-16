/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class PostTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPost();
    void testPost_data();
    void testPostMoreData();
    void testPostMoreData_data();
    void testExtraContentType();
    void testExtraContentType_data();
};

void PostTest::testPost_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QByteArray>("inputData");

    QTest::addRow("put") << "http://localhost:5000/post/bla" << QByteArray("<p>Hello, World!</p>");
}

void PostTest::testPost()
{
    QFETCH(QString, url);
    QFETCH(QByteArray, inputData);

    auto *job = KIO::http_post(QUrl(url), inputData);
    job->addMetaData("content-type", "text/html");

    connect(job, &KIO::TransferJob::mimeTypeFound, this, [] {
        qWarning() << "mime found";
    });

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());
    // QCOMPARE(dataReqCounter, 4);

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, "Got data of type text/html: <p>Hello, World!</p>");

    QCOMPARE(job->error(), KJob::NoError);
}

// TODO should this pass?
void PostTest::testPostMoreData_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QByteArray>("inputData");

    QTest::addRow("put") << "http://localhost:5000/post/bla" << QByteArray("<p>Hello, World!</p>");
}

void PostTest::testPostMoreData()
{
    QFETCH(QString, url);
    QFETCH(QByteArray, inputData);

    auto *job = KIO::http_post(QUrl(url), inputData);
    job->addMetaData("content-type", "text/plain");

    int dataReqCounter = 0;
    connect(job, &KIO::TransferJob::dataReq, this, [inputData, &dataReqCounter](KJob * /*job*/, QByteArray &data) {
        qWarning() << "dataReq";
        if (dataReqCounter < 3) {
            data = inputData;
        }
        dataReqCounter++;
    });

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);

    connect(job, &KIO::TransferJob::data, this, [] {
        qWarning() << "data";
    });

    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());
    QCOMPARE(dataReqCounter, 4);

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, "Got data of type text/plain: <p>Hello, World!</p><p>Hello, World!</p><p>Hello, World!</p><p>Hello, World!</p>");

    QCOMPARE(job->error(), KJob::NoError);
}

void PostTest::testExtraContentType_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QByteArray>("inputData");

    QTest::addRow("put") << "http://localhost:5000/post/bla" << QByteArray("<p>Hello, World!</p>");
}

void PostTest::testExtraContentType()
{
    QFETCH(QString, url);
    QFETCH(QByteArray, inputData);

    auto *job = KIO::http_post(QUrl(url), inputData);
    job->addMetaData("content-type", "Content-Type: text/html");

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, "Got data of type text/html: <p>Hello, World!</p>");

    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(PostTest)

#include "posttest.moc"
