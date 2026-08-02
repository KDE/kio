/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2005-2008 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2020 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KSYCOCATESTMENU_H
#define KSYCOCATESTMENU_H

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTest>

namespace KSycocaTestMenu
{

void setup()
{
    const QByteArray content = R"(<?xml version="1.0"?>
<!DOCTYPE Menu PUBLIC "-//freedesktop//DTD Menu 1.0//EN" "http://www.freedesktop.org/standards/menu-spec/menu-1.0.dtd">
<Menu>
  <Name>Applications</Name>
  <Directory>Applications.directory</Directory>
  <DefaultAppDirs/>
  <DefaultDirectoryDirs/>
  <MergeDir>applications-merged</MergeDir>
  <LegacyDir>/usr/share/applnk</LegacyDir>
  <DefaultLayout>
    <Merge type="menus"/>
    <Merge type="files"/>
    <Separator/>
    <Menuname>More</Menuname>
  </DefaultLayout>
</Menu>
)";

    const QString destDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/menus");
    QDir(destDir).mkpath(QStringLiteral("."));
    QFile output(destDir + QLatin1String("/applications.menu"));
    QVERIFY(output.open(QIODevice::ReadWrite | QIODevice::Truncate));
    output.write(content);
}

}

#endif
