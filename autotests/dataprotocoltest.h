/*
    SPDX-FileCopyrightText: 2012 Rolf Eike Beer <kde@opensource.sf-tec.de>
*/

#ifndef DATAPROTOCOLTEST_H
#define DATAPROTOCOLTEST_H

#include <kio/global.h>

#include <QObject>

class DataProtocolTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void runAllTests();
    void runAllTests_data();
};

#endif /* DATAPROTOCOLTEST_H */
