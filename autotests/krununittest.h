/*
 *  Copyright (C) 2005, 2009 David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
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

#ifndef KRUNUNITTEST_H
#define KRUNUNITTEST_H

#include <QObject>
#include <QStringList>
#include "kiowidgets_export.h"

class KRunUnitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testExecutableName_data();
    void testExecutableName();
    void testProcessDesktopExec();
    void testProcessDesktopExecNoFile_data();
    void testProcessDesktopExecNoFile();
    void testKtelnetservice();

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
    void testMimeTypeFile();
    void testMimeTypeDirectory();
    void testMimeTypeBrokenLink();
    void testMimeTypeDoesNotExist();

    void KRunRunService_data();
    void KRunRunService();
#endif
private:
    QString createTempService();

    QString m_sh;
    QString m_xterm;
    QStringList m_filesToRemove;

};

#endif /* KRUNUNITTEST_H */

