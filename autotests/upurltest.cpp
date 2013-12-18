/* This file is part of the KDE libraries
    Copyright (c) 2013 David Faure <faure@kde.org>

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

#include <qtest.h>
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
