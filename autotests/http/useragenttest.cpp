/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class UserAgentTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void UserAgentTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedMimeType");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("html") << "http://localhost:5000/useragent/enforce"
                          << "text/html" << QByteArray("Hello");
}

void UserAgentTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedMimeType);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url));

    job->addMetaData("UserAgent", "my test UA");

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);

    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(UserAgentTest)

#include "useragenttest.moc"
