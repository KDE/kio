/*

    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIO/TransferJob>

#include <QSignalSpy>
#include <QTest>

class RedirectTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testRedirectGet();
    void testRedirectGet_data();
    void testPermanentRedirect();
    void testPermanentRedirect_data();

    void testRedirectPost();
    void testRedirectPost_data();
    void testPermanentRedirectPost();
    void testPermanentRedirectPost_data();

    void testRedirectPut();
    void testRedirectPut_data();
    void testPermanentRedirectPut();
    void testPermanentRedirectPut_data();
};

void RedirectTest::testRedirectGet_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("redirectUrl");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("redirect") << "http://localhost:5000/get/redirect"
                              << "http://localhost:5000/get/redirected" << QByteArray("Itsa me, redirected\n");
    QTest::addRow("redirect") << "http://localhost:5000/get/redirect_303"
                              << "http://localhost:5000/get/redirected" << QByteArray("Itsa me, redirected\n");
    QTest::addRow("redirect") << "http://localhost:5000/get/redirect_307"
                              << "http://localhost:5000/get/redirected" << QByteArray("Itsa me, redirected\n");
}

void RedirectTest::testRedirectGet()
{
    QFETCH(QString, url);
    QFETCH(QString, redirectUrl);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url));

    QSignalSpy redirectSpy(job, &KIO::TransferJob::redirection);
    redirectSpy.wait();
    QVERIFY(redirectSpy.count());
    const QString actualRedirectUrl = redirectSpy.first().at(1).toString();
    QCOMPARE(actualRedirectUrl, redirectUrl);

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy finishedSpy(job, &KIO::TransferJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);
}

void RedirectTest::testPermanentRedirect_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("redirectUrl");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("redirect_301") << "http://localhost:5000/get/permanent_redirect"
                                  << "http://localhost:5000/get/permanent_redirected" << QByteArray("Itsa me, redirected permanently\n");
    QTest::addRow("redirect_308") << "http://localhost:5000/get/redirect_308"
                                  << "http://localhost:5000/get/permanent_redirected" << QByteArray("Itsa me, redirected permanently\n");
}

void RedirectTest::testPermanentRedirect()
{
    QFETCH(QString, url);
    QFETCH(QString, redirectUrl);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::get(QUrl(url));

    // Test redirection signal
    QSignalSpy redirectionSpy(job, &KIO::TransferJob::redirection);
    redirectionSpy.wait();
    QVERIFY(redirectionSpy.count());
    const QString actualRedirectUrl = redirectionSpy.first().at(1).toString();
    QCOMPARE(actualRedirectUrl, redirectUrl);

    // Test permanentRedirection signal
    QSignalSpy permanentRedirectionSpy(job, &KIO::TransferJob::permanentRedirection);
    permanentRedirectionSpy.wait();
    QVERIFY(permanentRedirectionSpy.count());

    const QString actualRedirectedFrom = permanentRedirectionSpy.first().at(1).toString();
    QCOMPARE(actualRedirectedFrom, url);

    const QString actualRedirectedTo = permanentRedirectionSpy.first().at(2).toString();
    QCOMPARE(actualRedirectedTo, redirectUrl);

    // Test data and finished signal
    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy finishedSpy(job, &KIO::TransferJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);
}

void RedirectTest::testRedirectPost_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("redirectUrl");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("redirect") << "http://localhost:5000/post/redirect"
                              << "http://localhost:5000/get/redirected" << QByteArray("Itsa me, redirected\n");
    QTest::addRow("redirect_303") << "http://localhost:5000/post/redirect_303"
                                  << "http://localhost:5000/get/redirected" << QByteArray("Itsa me, redirected\n");
    // TODO this should work but doesn't
    // QTest::addRow("redirect_307") << "http://localhost:5000/post/redirect_307"
    // << "http://localhost:5000/post/redirected" << QByteArray("Itsa me, redirected\n");
}

void RedirectTest::testRedirectPost()
{
    QFETCH(QString, url);
    QFETCH(QString, redirectUrl);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::http_post(QUrl(url), QByteArray());

    QSignalSpy redirectSpy(job, &KIO::TransferJob::redirection);
    redirectSpy.wait();
    QVERIFY(redirectSpy.count());
    const QString actualRedirectUrl = redirectSpy.first().at(1).toString();
    QCOMPARE(actualRedirectUrl, redirectUrl);

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy finishedSpy(job, &KIO::TransferJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);
}

void RedirectTest::testPermanentRedirectPost_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("redirectUrl");
    QTest::addColumn<QByteArray>("expectedData");

    QTest::addRow("redirect_301") << "http://localhost:5000/post/permanent_redirect"
                                  << "http://localhost:5000/get/permanent_redirected" << QByteArray("Itsa me, redirected permanently\n");

    // TODO this should work, but doesn't
    // QTest::addRow("redirect_308") << "http://localhost:5000/post/redirect_308"
    // << "http://localhost:5000/post/permanent_redirected" << QByteArray("Itsa me, redirected permanently\n");
}

void RedirectTest::testPermanentRedirectPost()
{
    QFETCH(QString, url);
    QFETCH(QString, redirectUrl);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::http_post(QUrl(url), QByteArray());

    // Test redirection signal
    QSignalSpy redirectionSpy(job, &KIO::TransferJob::redirection);
    redirectionSpy.wait();
    QVERIFY(redirectionSpy.count());
    const QString actualRedirectUrl = redirectionSpy.first().at(1).toString();
    QCOMPARE(actualRedirectUrl, redirectUrl);

    // Test permanentRedirection signal
    QSignalSpy permanentRedirectionSpy(job, &KIO::TransferJob::permanentRedirection);
    permanentRedirectionSpy.wait();
    QVERIFY(permanentRedirectionSpy.count());

    const QString actualRedirectedFrom = permanentRedirectionSpy.first().at(1).toString();
    QCOMPARE(actualRedirectedFrom, url);

    const QString actualRedirectedTo = permanentRedirectionSpy.first().at(2).toString();
    QCOMPARE(actualRedirectedTo, redirectUrl);

    // Test data and finished signal
    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy finishedSpy(job, &KIO::TransferJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);
}

void RedirectTest::testRedirectPut_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("redirectUrl");
    QTest::addColumn<QByteArray>("expectedData");

    // TODO what should happen here, redirect to GET or PUT?
    // QTest::addRow("redirect") << "http://localhost:5000/put/redirect"
    // << "http://localhost:5000/get/redirected" << QByteArray("Itsa me, redirected\n");
    // QTest::addRow("redirect_303") << "http://localhost:5000/put/redirect_303"
    // << "http://localhost:5000/get/redirected" << QByteArray("Itsa me, redirected\n");
    // TODO this should work but doesn't
    // QTest::addRow("redirect_307") << "http://localhost:5000/put/redirect_307"
    // << "http://localhost:5000/put/redirected" << QByteArray("Itsa me, redirected\n");
}

void RedirectTest::testRedirectPut()
{
    QSKIP("TODO clarify expected behavior");
    QFETCH(QString, url);
    QFETCH(QString, redirectUrl);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::put(QUrl(url), -1);

    QSignalSpy redirectSpy(job, &KIO::TransferJob::redirection);
    redirectSpy.wait();
    QVERIFY(redirectSpy.count());
    const QString actualRedirectUrl = redirectSpy.first().at(1).toString();
    QCOMPARE(actualRedirectUrl, redirectUrl);

    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy finishedSpy(job, &KIO::TransferJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);
}

void RedirectTest::testPermanentRedirectPut_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("redirectUrl");
    QTest::addColumn<QByteArray>("expectedData");

    // TODO what should happen here, redirect to GET or PUT?
    // QTest::addRow("redirect_301") << "http://localhost:5000/put/permanent_redirect"
    // << "http://localhost:5000/get/permanent_redirected" << QByteArray("Itsa me, redirected permanently\n");

    // TODO this should work, but doesn't
    // QTest::addRow("redirect_308") << "http://localhost:5000/put/redirect_308"
    // << "http://localhost:5000/put/permanent_redirected" << QByteArray("Itsa me, redirected permanently\n");
}

void RedirectTest::testPermanentRedirectPut()
{
    QSKIP("TODO clarify expected behavior");
    QFETCH(QString, url);
    QFETCH(QString, redirectUrl);
    QFETCH(QByteArray, expectedData);

    auto *job = KIO::put(QUrl(url), -1);

    // Test redirection signal
    QSignalSpy redirectionSpy(job, &KIO::TransferJob::redirection);
    redirectionSpy.wait();
    QVERIFY(redirectionSpy.count());
    const QString actualRedirectUrl = redirectionSpy.first().at(1).toString();
    QCOMPARE(actualRedirectUrl, redirectUrl);

    // Test permanentRedirection signal
    QSignalSpy permanentRedirectionSpy(job, &KIO::TransferJob::permanentRedirection);
    permanentRedirectionSpy.wait();
    QVERIFY(permanentRedirectionSpy.count());

    const QString actualRedirectedFrom = permanentRedirectionSpy.first().at(1).toString();
    QCOMPARE(actualRedirectedFrom, url);

    const QString actualRedirectedTo = permanentRedirectionSpy.first().at(2).toString();
    QCOMPARE(actualRedirectedTo, redirectUrl);

    // Test data and finished signal
    QSignalSpy dataSpy(job, &KIO::TransferJob::data);
    QSignalSpy finishedSpy(job, &KIO::TransferJob::finished);
    finishedSpy.wait();
    QVERIFY(finishedSpy.count());

    QVERIFY(dataSpy.count());
    const QByteArray actualData = dataSpy.first().at(1).toByteArray();
    QCOMPARE(actualData, expectedData);
}

QTEST_GUILESS_MAIN(RedirectTest)

#include "redirecttest.moc"
