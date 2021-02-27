/*
    SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "mydatasystem.h"

QByteArray DataItem::data() const
{
    // simulating content data fetched for the item
    return QByteArrayLiteral("Hello world\n");
}


MyDataSystem::MyDataSystem()
    // sample data simulating that in the exposed data system
    : m_toplevelGroup({
        { // subgroups
            { QStringLiteral("Subgroup"), {
                {}, // subgroups
                {   // items
                    { QStringLiteral("Item C") },
                    { QStringLiteral("Item D") },
                    { QStringLiteral("Item E") },
                }
            } }
        },
        { // items
            { QStringLiteral("Item A") },
            { QStringLiteral("Item B") },
        }
    })
{
}

const DataGroup* MyDataSystem::group(const QStringList& groupPath) const
{
    const DataGroup* currentGroup = &m_toplevelGroup;
    for (const QString &group : groupPath) {
        auto it = currentGroup->subGroups.constFind(group);
        if (it == currentGroup->subGroups.constEnd()) {
            return nullptr;
        }
        currentGroup = &*it;
    }
    return currentGroup;
}

bool MyDataSystem::hasGroup(const QStringList& groupPath) const
{
    return (group(groupPath) != nullptr);
}

QList<DataItem> MyDataSystem::items(const QStringList& groupPath) const
{
    auto* group = this->group(groupPath);
    return group ? group->items : QList<DataItem>();
}

DataItem MyDataSystem::item(const QStringList& groupPath, const QString &itemName) const
{
    auto* group = this->group(groupPath);
    if (group) {
        for (auto &item : group->items) {
            if (item.name == itemName) {
                return item;
            }
        }
    }
    return DataItem();
}

QStringList MyDataSystem::subGroupNames(const QStringList& groupPath) const
{
    auto* group = this->group(groupPath);
    return group ? group->subGroups.keys() : QStringList();
}

