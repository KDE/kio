/*
    SPDX-FileCopyrightText: 2022 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KRECENTDOCUMENTTEST_H
#define KRECENTDOCUMENTTEST_H

#include <QObject>

class KRecentDocumentTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void testXbelBookmark();

private:
    QString m_xbelPath;
    QString m_testFile;
};

#endif // KRECENTDOCUMENTTEST_H
