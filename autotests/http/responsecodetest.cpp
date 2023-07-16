/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class ResponseCodeTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
};

void ResponseCodeTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedCode");

    QTest::addRow("200") << "http://localhost:5000/get/html"
                         << "200";
    QTest::addRow("404") << "http://localhost:5000/get/does-not-exist"
                         << "404";
}

void ResponseCodeTest::testGet()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedCode);

    auto *job = KIO::get(QUrl(url));

    QSignalSpy spy(job, &KJob::finished);
    spy.wait();
    QVERIFY(spy.size());

    const QString actualCode = job->queryMetaData("responsecode");

    QCOMPARE(actualCode, expectedCode);

    QCOMPARE(job->error(), KJob::NoError);
}

QTEST_GUILESS_MAIN(ResponseCodeTest)

#include "responsecodetest.moc"
