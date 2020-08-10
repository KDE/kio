/*
    This file is part of KDE Frameworks
    SPDX-FileCopyrightText: 2005-2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIOPASTETEST_H
#define KIOPASTETEST_H

#include <QObject>
#include <QTemporaryDir>
#include <QString>

class KIOPasteTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();

    void testPopulate();
    void testCut();
    void testPasteActionText_data();
    void testPasteActionText();
    void testPasteJob_data();
    void testPasteJob();

private:
    QTemporaryDir m_tempDir;
    QString m_dir;
};

#endif
