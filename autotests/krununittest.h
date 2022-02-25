/*
    SPDX-FileCopyrightText: 2005, 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KRUNUNITTEST_H
#define KRUNUNITTEST_H

#include "kiowidgets_export.h"
#include <QObject>
#include <QStringList>

class KRunUnitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

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

    QStringList m_filesToRemove;
};

#endif /* KRUNUNITTEST_H */
