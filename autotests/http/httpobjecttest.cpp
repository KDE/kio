/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 Rolf Eike Beer <kde@opensource.sf-tec.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "httpobjecttest.h"

#include <QTest>

#include <QByteArray>

QTEST_MAIN(HeaderObjectTest)

static void runTest()
{
    TestHTTPProtocol protocol("http", QByteArray(), "local://");

    protocol.testParseContentDisposition(QStringLiteral("inline; filename=\"foo.pdf\""));
}

void HeaderObjectTest::runAllTests()
{
    runTest();
}

TestHTTPProtocol::TestHTTPProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app)
    : HTTPProtocol(protocol, pool, app)
{
}

TestHTTPProtocol::~TestHTTPProtocol()
{
}

void TestHTTPProtocol::testParseContentDisposition(const QString &disposition)
{
    parseContentDisposition(disposition);
}
