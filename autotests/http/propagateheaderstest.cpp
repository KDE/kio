/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class PropagateHeadersTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void PropagateHeadersTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedMimeType");
    QTest::addColumn<QByteArray>("expectedData");
    QTest::addColumn<QStringList>("expectedHeaders");

    QTest::addRow("html") << "http://localhost:5000/get/html"
                          << "text/html" << QByteArray("<p>Hello, World!</p>")
                          << QStringList{"server: Werkzeug/2.3.6 Python/3.11.4",
                                         "date: Wed, 09 Aug 2023 15:07:45 GMT",
                                         "content-type: text/html; charset=utf-8",
                                         "content-length: 20",
                                         "connection: close"};
    QTest::addRow("calendar") << "http://localhost:5000/get/calendar"
                              << "text/calendar" << QByteArray("Some data\nthat\nhas\nnew\nlines\n")
                              << QStringList{"server: Werkzeug/2.3.6 Python/3.11.4",
                                             "date: Wed, 09 Aug 2023 15:22:24 GMT",
                                             "content-type: text/calendar; charset=utf-8",
                                             "content-length: 29",
                                             "connection: close"};
}

void PropagateHeadersTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedMimeType);
    QFETCH(QByteArray, expectedData);
    QFETCH(QStringList, expectedHeaders);

    auto *job = KIO::get(QUrl(url));
    job->addMetaData(QStringLiteral("PropagateHttpHeader"), QStringLiteral("true"));

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
    const QStringList headers = job->queryMetaData(QStringLiteral("HTTP-Headers")).split("\n");

    qWarning() << job->queryMetaData(QStringLiteral("HTTP-Headers"));

    for (int i = 0; i < headers.length(); ++i) {
        if (headers[i].startsWith("date:", Qt::CaseInsensitive)) {
            continue;
        }

        if (headers[i].startsWith("server:", Qt::CaseInsensitive)) {
            continue;
        }

        QCOMPARE(headers[i].toLower(), expectedHeaders[i].toLower());
    }
}

QTEST_GUILESS_MAIN(PropagateHeadersTest)

#include "propagateheaderstest.moc"
