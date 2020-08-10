/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Henri Chain <henri.chain@enioka.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
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
