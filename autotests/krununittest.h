/*
    SPDX-FileCopyrightText: 2005, 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
    QString m_pseudoTerminalProgram;
    QStringList m_filesToRemove;

};

#endif /* KRUNUNITTEST_H */

