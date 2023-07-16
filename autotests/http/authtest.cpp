/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class AuthTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void AuthTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedMimeType");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("html") << "http://localhost:5000/auth/test"
                          << "text/html" << QByteArray("Hello");
}

void AuthTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedMimeType);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url));

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy spy(job, &KJob::finished);
    spy.wait(-1);
    QVERIFY(spy.size());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);

    qWarning() << "error" << job->error();

    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(AuthTest)

#include "authtest.moc"
