/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2011 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef HTTPAUTHENTICATIONTEST_H
#define HTTPAUTHENTICATIONTEST_H

#include <QObject>

class HTTPAuthenticationTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testHeaderParsing();
    void testHeaderParsing_data();
    void testAuthenticationSelection();
    void testAuthenticationSelection_data();
    void testAuthentication();
    void testAuthentication_data();
    void testAuthenticationNTLMv2();
};

#endif
