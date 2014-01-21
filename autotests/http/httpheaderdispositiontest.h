/* This file is part of the KDE libraries
    Copyright (c) 2010 Rolf Eike Beer <kde@opensource.sf-tec.de>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef HTTPHEADERDISPOSITIONTEST_H
#define HTTPHEADERDISPOSITIONTEST_H

#include <QtCore/QObject>

class HeaderDispositionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void runAllTests();
    void runAllTests_data();
};

#endif //HTTPHEADERDISPOSITIONTEST_H
