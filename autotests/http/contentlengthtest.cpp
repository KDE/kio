/*

    SPDX-FileCopyrightText: 2025 Robby Stephenson <robby@periapsis.org

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class ContentLengthTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRequest();
    void testRequest_data();
};

void ContentLengthTest::testRequest_data()
{
    QTest::addColumn<QString>("requestType");
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedCode");

    QTest::addRow("get") << "GET" << "http://localhost:5000/content-length" << "200";
    QTest::addRow("post") << "POST" << "http://localhost:5000/content-length" << "200";
}

void ContentLengthTest::testRequest()
{
    QFETCH(QString, requestType);
    QFETCH(QString, url);
    QFETCH(QString, expectedCode);

    KIO::TransferJob *job = nullptr;
    if (requestType == "GET") {
        job = KIO::get(QUrl(url));
    } else if (requestType == "POST") {
        job = KIO::http_post(QUrl(url), QByteArray());
    }
    QVERIFY(job);

    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());

    const QString actualCode = job->queryMetaData("responsecode");
    QCOMPARE(actualCode, expectedCode);

    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(ContentLengthTest)

#include "contentlengthtest.moc"
