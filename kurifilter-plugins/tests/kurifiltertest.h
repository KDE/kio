/*
 *  Copyright (C) 2006 David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef KURIFILTERTEST_H
#define KURIFILTERTEST_H

#include <QObject>
#include <QByteArray>
#include <QStringList>

class KUriFilterTest : public QObject
{
    Q_OBJECT
public:
    KUriFilterTest();

private slots:
    void init();
    void noFiltering_data();
    void noFiltering();
    void localFiles_data();
    void localFiles();
    void shortUris_data();
    void shortUris();
    void refOrQuery_data();
    void refOrQuery();
    void executables_data();
    void executables();
    void environmentVariables_data();
    void environmentVariables();
    void internetKeywords_data();
    void internetKeywords();
    void localdomain();

private:
    QStringList minicliFilters;
    QByteArray qtdir;
    QByteArray home;
    QByteArray datahome;

};

#endif
