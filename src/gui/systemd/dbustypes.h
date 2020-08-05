/*
    This file is part of the KDE libraries
    Copyright (c) 2020 Henri Chain <henri.chain@enioka.com>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef DBUSTYPES_H
#define DBUSTYPES_H

#include <QDBusArgument>

using QVariantMultiItem = struct {
    QString key;
    QVariant value;
};
Q_DECLARE_METATYPE(QVariantMultiItem)
using QVariantMultiMap = QList<QVariantMultiItem>;
Q_DECLARE_METATYPE(QVariantMultiMap)

inline QDBusArgument &operator<<(QDBusArgument &argument, const QVariantMultiItem &item)
{
    argument.beginStructure();
    argument << item.key << QDBusVariant(item.value);
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, QVariantMultiItem &item)
{
    argument.beginStructure();
    argument >> item.key >> item.value;
    argument.endStructure();
    return argument;
}

using ExecCommand = struct {
    QString path;
    QStringList argv;
    bool ignoreFailure;
};
Q_DECLARE_METATYPE(ExecCommand)
using ExecCommandList = QList<ExecCommand>;
Q_DECLARE_METATYPE(ExecCommandList)

inline QDBusArgument &operator<<(QDBusArgument &argument, const ExecCommand &execCommand)
{
    argument.beginStructure();
    argument << execCommand.path << execCommand.argv << execCommand.ignoreFailure;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, ExecCommand &execCommand)
{
    argument.beginStructure();
    argument >> execCommand.path >> execCommand.argv >> execCommand.ignoreFailure;
    return argument;
}

using TransientAux = struct {
    QString name;
    QVariantMultiMap properties;
};
Q_DECLARE_METATYPE(TransientAux)
using TransientAuxList = QList<TransientAux>;
Q_DECLARE_METATYPE(TransientAuxList)

inline QDBusArgument &operator<<(QDBusArgument &argument, const TransientAux &aux)
{
    argument.beginStructure();
    argument << aux.name << aux.properties;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, TransientAux &aux)
{
    argument.beginStructure();
    argument >> aux.name >> aux.properties;
    argument.endStructure();
    return argument;
}

#endif // DBUSTYPES_H
