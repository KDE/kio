/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class AcceptTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void AcceptTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("accept");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("html") << "http://localhost:5000/accept/rss"
                          << QStringLiteral("application/rss+xml;q=0.9, application/atom+xml;q=0.9, text/*;q=0.8, */*;q=0.7") << QByteArray("Hello");
}

void AcceptTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QString, accept);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url));

    job->addMetaData(QStringLiteral("accept"), accept);

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);

    qWarning() << job->error() << job->errorString();
    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(AcceptTest)

#include "accepttest.moc"
