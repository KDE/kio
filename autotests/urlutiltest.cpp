/***************************************************************************
 * Copyright (C) 2016 by Gregor Mi <codestruct@posteo.org>                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA            *
 ***************************************************************************/

#include <QTest>

#include "../src/filewidgets/urlutil_p.h"

class UrlUtilTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFirstChildUrl();
};

static inline QUrl lUrl(const QString &path) { return QUrl::fromLocalFile(path); }

void UrlUtilTest::testFirstChildUrl()
{
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/data/documents/muh/"), lUrl("/home/test/")), lUrl("/home/test/data"));
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/data/documents/muh"), lUrl("/home/test")), lUrl("/home/test/data"));
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/data/documents/muh/"), lUrl("/home/test")), lUrl("/home/test/data"));
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/data/documents/muh"), lUrl("/home/test/")), lUrl("/home/test/data"));
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/data"), lUrl("/home/test/data/")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/"), lUrl("/home/test/")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/aaa/"), lUrl("/home/bbb/")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/aaa/"), lUrl("/home/bbb/ccc")), QUrl());
    // # is %23 in an URL, so this test could reveal path/URL confusion in the code:
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/a#/b#"), lUrl("/home/a#")), lUrl("/home/a#/b#"));
}

QTEST_MAIN(UrlUtilTest)

#include "urlutiltest.moc"
