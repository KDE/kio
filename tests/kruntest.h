/*
    SPDX-FileCopyrightText: 2002 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _kruntest_h
#define _kruntest_h

#include <QWidget>

class QPushButton;
class Receiver : public QWidget
{
    Q_OBJECT
public:
    Receiver();
    ~Receiver() override
    {
    }
public Q_SLOTS:
    void slotStart();
    void slotStop();
    void slotLaunchOne();
    void slotLaunchTest(QPushButton *sender);

private:
    QPushButton *start;
    QPushButton *stop;
};

#endif
