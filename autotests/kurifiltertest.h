/*
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
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

private Q_SLOTS:
    void initTestCase();
    void pluginNames();
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
    void relativeGoUp();

private:
    QStringList minicliFilters;
    QByteArray qtdir;
    QByteArray home;
    QByteArray datahome;

};

#endif
