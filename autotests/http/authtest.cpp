/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/MimetypeJob>
#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class AuthTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testGet();
    void testGet_data();
    void testMimeType();
    void testMimeType_data();
};

void AuthTest::testGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedMimeType");
    QTest::addColumn<QByteArray>("expectedData");

    // Error page will also be text/html, use something else.
    QTest::addRow("markdown") << "http://localhost:5000/auth/test" << "text/markdown" << QByteArray("# Hello");
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

void AuthTest::testMimeType_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedMimeType");

    QTest::addRow("markdown") << "http://localhost:5000/auth/test" << "text/markdown";
}

void AuthTest::testMimeType()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedMimeType);

    // KIO::MimeTypeFinderJob does a GET request,
    // only KIO::mimetype does a HEAD which could break with auth.
    auto *job = KIO::mimetype(QUrl(url), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->mimetype(), expectedMimeType);
}

QTEST_GUILESS_MAIN(AuthTest)

#include "authtest.moc"
