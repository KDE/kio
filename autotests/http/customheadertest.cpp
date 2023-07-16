/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class CustomHeaderTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void CustomHeaderTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("headers");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("html") << "http://localhost:5000/headers/pineapple" << QStringLiteral("Pineapple: Ananas") << QByteArray("Hello");

    QTest::addRow("html") << "http://localhost:5000/headers/pizza" << QStringLiteral("Pineapple: Ananas\r\nPizza: yes") << QByteArray("🤌");
}

void CustomHeaderTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QString, headers);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url));

    job->addMetaData(QStringLiteral("customHTTPHeader"), headers);

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

QTEST_GUILESS_MAIN(CustomHeaderTest)

#include "customheadertest.moc"
