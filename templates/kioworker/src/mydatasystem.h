/*
    SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef MYDATASYSTEM_H
#define MYDATASYSTEM_H

// Qt
#include <QHash>
#include <QStringList>
#include <QByteArray>

// A sample class transporting the system data structure that would be mapped onto a file
class DataItem
{
public:
    QString name;
    QByteArray data() const;
    bool isValid() const { return !name.isEmpty(); }
};

// A sample class transporting the system data structure that would be mapped onto a directory
class DataGroup
{
public:
    QHash<QString, DataGroup> subGroups;
    QList<DataItem> items;
};


// A sample data system adapter 
class MyDataSystem
{
public:
    MyDataSystem();

public: // sync calls querying data from the actual data system
    bool hasGroup(const QStringList &groupPath) const;
    QList<DataItem> items(const QStringList &groupPath) const;
    DataItem item(const QStringList& groupPath, const QString &itemName) const;
    QStringList subGroupNames(const QStringList &groupPath) const;

private:
    const DataGroup* group(const QStringList& groupPath) const;

private:
    // hardcoded sample data to simulate that in the data system
    DataGroup m_toplevelGroup;
};

#endif
