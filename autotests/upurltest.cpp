/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>
#include <kio/global.h>

class KIOUpUrlTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void upUrl_data();
    void upUrl();
};

void KIOUpUrlTest::upUrl_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("upUrl");

    QTest::newRow("empty") << "" << "";
    QTest::newRow("ref") << "file:/home/dfaure/my#myref" << "file:///home/dfaure/";
    QTest::newRow("qt2") << "file:/opt/kde2/qt2/doc/html/showimg-main-cpp.html#QObject::connect" << "file:///opt/kde2/qt2/doc/html/";
    QTest::newRow("query") << "http://www.kde.org/cgi/test.cgi?hello:My Value" << "http://www.kde.org/cgi/test.cgi";
    QTest::newRow("ftp1") << "ftp://user%40host.com@ftp.host.com/var/www/" << "ftp://user%40host.com@ftp.host.com/var/";
    QTest::newRow("ftp2") << "ftp://user%40host.com@ftp.host.com/var/" << "ftp://user%40host.com@ftp.host.com/";
    QTest::newRow("ftp3") << "ftp://user%40host.com@ftp.host.com/" << "ftp://user%40host.com@ftp.host.com/"; // unchanged
    QTest::newRow("relative") << "tmp" << ""; // Going up from a relative url is not supported (#170695)
}

void KIOUpUrlTest::upUrl()
{
    QFETCH(QString, url);
    QFETCH(QString, upUrl);

    QCOMPARE(KIO::upUrl(QUrl(url)).toString(), upUrl);
}

QTEST_MAIN(KIOUpUrlTest)

#include "upurltest.moc"
