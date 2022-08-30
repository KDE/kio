/*
    SPDX-FileCopyrightText: 2005, 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DESKTOPEXECPARSERTEST_H
#define DESKTOPEXECPARSERTEST_H

#include <QObject>

class DesktopExecParserTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testExecutableName_data();
    void testExecutableName();
    void testProcessDesktopExec();
    void testProcessDesktopExecNoFile_data();
    void testProcessDesktopExecNoFile();
    void testKtelnetservice();

private:
    QString m_sh;
    QString m_pseudoTerminalProgram;
};

#endif /* DESKTOPEXECPARSERTEST_H */
