/*
    SPDX-FileCopyrightText: 2016 Gregor Mi <codestruct@posteo.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/a/"), lUrl("/home")), lUrl("/home/a"));
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/te"), lUrl("/")), lUrl("/te"));
    // One letter under root is also a valid child
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/d"), lUrl("/")), lUrl("/d"));
    // Same urls should return QUrl()
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/data"), lUrl("/home/test/data/")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/data/"), lUrl("/home/test/data")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/test/"), lUrl("/home/test/")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/"), lUrl("/")), QUrl());
    // Not related urls should return QUrl()
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/aaa/"), lUrl("/home/bbb/")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/aaa/"), lUrl("/home/bbb/ccc")), QUrl());
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home"), lUrl("/test")), QUrl());
    // Child urls in reverse order should return QUrl()
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home"), lUrl("/home/test")), QUrl());
    // # is %23 in an URL, so this test could reveal path/URL confusion in the code:
    QCOMPARE(KIO::UrlUtil::firstChildUrl(lUrl("/home/a#/b#"), lUrl("/home/a#")), lUrl("/home/a#/b#"));
}

QTEST_MAIN(UrlUtilTest)

#include "urlutiltest.moc"
