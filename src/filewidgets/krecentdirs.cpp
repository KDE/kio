/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: BSD-2-Clause
*/

#include "krecentdirs.h"
#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#define MAX_DIR_HISTORY 3

static KConfigGroup recentdirs_readList(QString &key, QStringList &result)
{
    KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("Recent Dirs"));
    if ((key.length() < 2) || (key[0] != QLatin1Char(':'))) {
        key = QStringLiteral(":default");
    }
    if (key[1] == QLatin1Char(':')) {
        key.remove(0, 2);
        cg = KConfigGroup(KSharedConfig::openConfig(QStringLiteral("krecentdirsrc")), QString());
    } else {
        key.remove(0, 1);
    }

    result = cg.readPathEntry(key, QStringList());
    if (result.isEmpty()) {
        result.append(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    }
    return cg;
}

QStringList KRecentDirs::list(const QString &fileClass)
{
    QString key = fileClass;
    QStringList result;
    recentdirs_readList(key, result).sync();
    return result;
}

QString KRecentDirs::dir(const QString &fileClass)
{
    const QStringList result = list(fileClass);
    return result[0];
}

void KRecentDirs::add(const QString &fileClass, const QString &directory)
{
    QString key = fileClass;
    QStringList result;
    KConfigGroup config = recentdirs_readList(key, result);
    // make sure the dir is first in history
    result.removeAll(directory);
    result.prepend(directory);
    while (result.count() > MAX_DIR_HISTORY) {
        result.removeLast();
    }
    config.writePathEntry(key, result);
    config.sync();
}

