/* This file is part of the KDE libraries
    Copyright (c) 2016 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <kio/job.h>

#include <QDebug>
#include <QTest>
#include <QSignalSpy>
#include <QStandardPaths>

#include "httpserver_p.h"

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

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");
    // To let ctest exit, we shouldn't start kio_http_cache_cleaner
    qputenv("KIO_DISABLE_CACHE_CLEANER", "yes");
}

void HTTPJobTest::testBasicGet()
{
    static const char response[] = "Hello world";
    HttpServerThread server(response, HttpServerThread::Public);
    KIO::StoredTransferJob *job = KIO::storedGet(QUrl(server.endPoint()));
    job->setUiDelegate(0);
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
    job->setUiDelegate(0);
    QVERIFY(job->exec());
    QCOMPARE(QString::fromLatin1(job->data()), QString::fromLatin1(response));
    QVERIFY(job->isErrorPage());
    QCOMPARE(job->error(), 0);

    // Second we disable error page, and get the actual job error
    job = KIO::storedGet(QUrl(server.endPoint()));
    job->setUiDelegate(0);
    job->addMetaData(QStringLiteral("errorPage"), QStringLiteral("false")); // maybe this should be a proper setter...
    QVERIFY(!job->exec());
    QVERIFY(!job->isErrorPage());
    QCOMPARE(job->error(), int(KIO::ERR_DOES_NOT_EXIST));

    // To check that kio_http did read and discard the body correctly, do another working download.
    server.setResponseData("<html>Some HTML page here</html>");
    server.setFeatures(HttpServerThread::Public);
    server.setContentType("");
    job = KIO::storedGet(QUrl(server.endPoint()));
    job->setUiDelegate(0);
    QSignalSpy mimeTypeSpy(job, SIGNAL(mimetype(KIO::Job*,QString)));
    QVERIFY(job->exec());
    QCOMPARE(job->error(), 0);
    QCOMPARE(mimeTypeSpy.count(), 1);
    QCOMPARE(mimeTypeSpy.at(0).at(1).toString(), QStringLiteral("text/html"));
}

void HTTPJobTest::testMimeTypeDetermination()
{
    static const char response[] = "<html>Some HTML page here</html>";
    HttpServerThread server(response, HttpServerThread::Public);
    // Add a trailing slash to ensure kio_http doesn't confuse QMimeDatabase with it.
    KIO::StoredTransferJob *job = KIO::storedGet(QUrl(server.endPoint() + '/'));
    job->setUiDelegate(0);
    QSignalSpy mimeTypeSpy(job, SIGNAL(mimetype(KIO::Job*,QString)));
    QVERIFY(job->exec());
    QCOMPARE(job->error(), 0);
    QCOMPARE(mimeTypeSpy.count(), 1);
    QCOMPARE(mimeTypeSpy.at(0).at(1).toString(), QStringLiteral("text/html"));
}

QTEST_MAIN(HTTPJobTest)
#include "http_jobtest.moc"
