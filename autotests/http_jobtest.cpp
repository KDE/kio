/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "httpserver_p.h"
#include <kio/storedtransferjob.h>

class HTTPJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testBasicGet();
    void testErrorPage();
    void testMimeTypeDetermination();
};

void HTTPJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void HTTPJobTest::testBasicGet()
{
    static const char response[] = "Hello world";
    HttpServerThread server(response, HttpServerThread::Public);
    KIO::StoredTransferJob *job = KIO::storedGet(QUrl(server.endPoint()));
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());
    QCOMPARE(QString::fromLatin1(job->data()), QString::fromLatin1(response));
}

void HTTPJobTest::testErrorPage()
{
    static const char response[] = "<html>This is a response\nFile not found</html>";
    HttpServerThread server(response, HttpServerThread::Error404);
    server.setContentType("text/html");

    // First we get an error page
    KIO::StoredTransferJob *job = KIO::storedGet(QUrl(server.endPoint()));
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());
    QCOMPARE(QString::fromLatin1(job->data()), QString::fromLatin1(response));
    QVERIFY(job->isErrorPage());
    QCOMPARE(job->error(), 0);

    // Second we disable error page, and get the actual job error
    job = KIO::storedGet(QUrl(server.endPoint()));
    job->setUiDelegate(nullptr);
    job->addMetaData(QStringLiteral("errorPage"), QStringLiteral("false")); // maybe this should be a proper setter...
    QVERIFY(!job->exec());
    QVERIFY(!job->isErrorPage());
    QCOMPARE(job->error(), int(KIO::ERR_DOES_NOT_EXIST));

    // To check that kio_http did read and discard the body correctly, do another working download.
    server.setResponseData("<html>Some HTML page here</html>");
    server.setFeatures(HttpServerThread::Public);
    server.setContentType("");
    job = KIO::storedGet(QUrl(server.endPoint()));
    job->setUiDelegate(nullptr);
    QSignalSpy mimeTypeFoundSpy(job, &KIO::TransferJob::mimeTypeFound);
    QVERIFY(job->exec());
    QCOMPARE(job->error(), 0);
    QCOMPARE(mimeTypeFoundSpy.count(), 1);
    QCOMPARE(mimeTypeFoundSpy.at(0).at(1).toString(), QStringLiteral("text/html"));
}

void HTTPJobTest::testMimeTypeDetermination()
{
    static const char response[] = "<html>Some HTML page here</html>";
    HttpServerThread server(response, HttpServerThread::Public);
    // Add a trailing slash to ensure kio_http doesn't confuse QMimeDatabase with it.
    KIO::StoredTransferJob *job = KIO::storedGet(QUrl(server.endPoint() + '/'));
    job->setUiDelegate(nullptr);
    QSignalSpy mimeTypeFoundSpy(job, &KIO::TransferJob::mimeTypeFound);
    QVERIFY(job->exec());
    QCOMPARE(job->error(), 0);
    QCOMPARE(mimeTypeFoundSpy.count(), 1);
    QCOMPARE(mimeTypeFoundSpy.at(0).at(1).toString(), QStringLiteral("text/html"));
}

QTEST_MAIN(HTTPJobTest)
#include "http_jobtest.moc"
