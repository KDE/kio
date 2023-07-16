/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class GetTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void GetTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedMimeType");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("html") << "http://localhost:5000/get/html"
                          << "text/html" << QByteArray("<p>Hello, World!</p>");
    QTest::addRow("calendar") << "http://localhost:5000/get/calendar"
                              << "text/calendar" << QByteArray("Some data\nthat\nhas\nnew\nlines\n");
}

void GetTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedMimeType);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url));

    QSignalSpy mimeTypeFoundSpy(job, &KIO::TransferJob::mimeTypeFound);
    mimeTypeFoundSpy.wait();
    QCOMPARE(mimeTypeFoundSpy.count(), 1);

    auto args = mimeTypeFoundSpy.first();
    QCOMPARE(args[1], expectedMimeType);

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());
    QCOMPARE(job->mimetype(), expectedMimeType);

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);

    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(GetTest)

#include "gettest.moc"
