/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 Rolf Eike Beer <kde@opensource.sf-tec.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef HTTPOBJECTTEST_H
#define HTTPOBJECTTEST_H

#include <QObject>
#include <http.h>

class HeaderObjectTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void runAllTests();
};

class TestHTTPProtocol : public HTTPProtocol
{
    Q_OBJECT
public:
    TestHTTPProtocol(const QByteArray &protocol, const QByteArray &pool,
                     const QByteArray &app);
    virtual ~TestHTTPProtocol();

    void testParseContentDisposition(const QString &disposition);
};

#endif //HTTPOBJECTTEST_H
